/*
 * kexec: Linux boots Linux
 *
 * Created by: Vivek Goyal (vgoyal@in.ibm.com)
 * old x86_64 version Created by: Murali M Chakravarthy (muralim@in.ibm.com)
 * Copyright (C) IBM Corporation, 2005. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation (version 2 of the License).
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define _XOPEN_SOURCE	600
#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <elf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"
#include "../../kexec-syscall.h"
#include "../../firmware_memmap.h"
#include "../../crashdump.h"
#include "kexec-x86.h"
#include "crashdump-x86.h"
#include "../../kexec-xen.h"
#include "x86-linux-setup.h"
#include <x86/x86-linux.h>

extern struct arch_options_t arch_options;

static int get_kernel_page_offset(struct kexec_info *UNUSED(info),
				  struct crash_elf_info *elf_info)
{

	if (elf_info->machine == EM_X86_64) {
		/* get_kernel_vaddr_and_size will override this */
		elf_info->page_offset = X86_64_PAGE_OFFSET;
	}
	else if (elf_info->machine == EM_386) {
		elf_info->page_offset = X86_PAGE_OFFSET;
	}

	return 0;
}

#define X86_64_KERN_VADDR_ALIGN	0x100000	/* 1MB */

/* Read kernel physical load addr from the file returned by proc_iomem()
 * (Kernel Code) and store in kexec_info */
static int get_kernel_paddr(struct kexec_info *UNUSED(info),
			    struct crash_elf_info *elf_info)
{
	uint64_t start;

	if (elf_info->machine != EM_X86_64)
		return 0;

	if (xen_present()) /* Kernel not entity mapped under Xen */
		return 0;

	if (parse_iomem_single("Kernel code\n", &start, NULL) == 0) {
		elf_info->kern_paddr_start = start;
		dbgprintf("kernel load physical addr start = 0x%016Lx\n",
			  (unsigned long long)start);
		return 0;
	}

	fprintf(stderr, "Cannot determine kernel physical load addr\n");
	return -1;
}

/* Retrieve info regarding virtual address kernel has been compiled for and
 * size of the kernel from /proc/kcore. Current /proc/kcore parsing from
 * from kexec-tools fails because of malformed elf notes. A kernel patch has
 * been submitted. For the folks using older kernels, this function
 * hard codes the values to remain backward compatible. Once things stablize
 * we should get rid of backward compatible code. */

static int get_kernel_vaddr_and_size(struct kexec_info *UNUSED(info),
				     struct crash_elf_info *elf_info)
{
	int result;
	const char kcore[] = "/proc/kcore";
	char *buf;
	struct mem_ehdr ehdr;
	struct mem_phdr *phdr, *end_phdr;
	int align;
	off_t size;
	uint32_t elf_flags = 0;
	uint64_t stext_sym;
	const unsigned long long pud_mask = ~((1 << 30) - 1);
	unsigned long long vaddr, lowest_vaddr = 0;

	if (elf_info->machine != EM_X86_64)
		return 0;

	if (xen_present()) /* Kernel not entity mapped under Xen */
		return 0;

	align = getpagesize();
	buf = slurp_file_len(kcore, KCORE_ELF_HEADERS_SIZE, &size);
	if (!buf) {
		fprintf(stderr, "Cannot read %s: %s\n", kcore, strerror(errno));
		return -1;
	}

	/* Don't perform checks to make sure stated phdrs and shdrs are
	 * actually present in the core file. It is not practical
	 * to read the GB size file into a user space buffer, Given the
	 * fact that we don't use any info from that.
	 */
	elf_flags |= ELF_SKIP_FILESZ_CHECK;
	result = build_elf_core_info(buf, size, &ehdr, elf_flags);
	if (result < 0) {
		/* Perhaps KCORE_ELF_HEADERS_SIZE is too small? */
		fprintf(stderr, "ELF core (kcore) parse failed\n");
		return -1;
	}

	end_phdr = &ehdr.e_phdr[ehdr.e_phnum];

	/* Search for the real PAGE_OFFSET when KASLR memory randomization
	 * is enabled */
	for(phdr = ehdr.e_phdr; phdr != end_phdr; phdr++) {
		if (phdr->p_type == PT_LOAD) {
			vaddr = phdr->p_vaddr & pud_mask;
			if (lowest_vaddr == 0 || lowest_vaddr > vaddr)
				lowest_vaddr = vaddr;
		}
	}
	if (lowest_vaddr != 0)
		elf_info->page_offset = lowest_vaddr;

	/* Traverse through the Elf headers and find the region where
	 * _stext symbol is located in. That's where kernel is mapped */
	stext_sym = get_kernel_sym("_stext");
	for(phdr = ehdr.e_phdr; stext_sym && phdr != end_phdr; phdr++) {
		if (phdr->p_type == PT_LOAD) {
			unsigned long long saddr = phdr->p_vaddr;
			unsigned long long eaddr = phdr->p_vaddr + phdr->p_memsz;
			unsigned long long size;

			/* Look for kernel text mapping header. */
			if (saddr <= stext_sym && eaddr > stext_sym) {
				saddr = _ALIGN_DOWN(saddr, X86_64_KERN_VADDR_ALIGN);
				elf_info->kern_vaddr_start = saddr;
				size = eaddr - saddr;
				/* Align size to page size boundary. */
				size = _ALIGN(size, align);
				elf_info->kern_size = size;
				dbgprintf("kernel vaddr = 0x%llx size = 0x%llx\n",
					saddr, size);
				return 0;
			}
		}
	}

	/* If failed to retrieve kernel text mapping through
	 * /proc/kallsyms, Traverse through the Elf headers again and
	 * find the region where kernel is mapped using hard-coded
	 * kernel mapping boundries */
	for(phdr = ehdr.e_phdr; phdr != end_phdr; phdr++) {
		if (phdr->p_type == PT_LOAD) {
			unsigned long long saddr = phdr->p_vaddr;
			unsigned long long eaddr = phdr->p_vaddr + phdr->p_memsz;
			unsigned long long size;

			/* Look for kernel text mapping header. */
			if ((saddr >= X86_64__START_KERNEL_map) &&
			    (eaddr <= X86_64__START_KERNEL_map + X86_64_KERNEL_TEXT_SIZE)) {
				saddr = _ALIGN_DOWN(saddr, X86_64_KERN_VADDR_ALIGN);
				elf_info->kern_vaddr_start = saddr;
				size = eaddr - saddr;
				/* Align size to page size boundary. */
				size = _ALIGN(size, align);
				elf_info->kern_size = size;
				dbgprintf("kernel vaddr = 0x%llx size = 0x%llx\n",
					saddr, size);
				return 0;
			}
		}
	}

	fprintf(stderr, "Can't find kernel text map area from kcore\n");
	return -1;
}

/* Forward Declaration. */
static void segregate_lowmem_region(int *nr_ranges, unsigned long lowmem_limit);
static int exclude_region(int *nr_ranges, uint64_t start, uint64_t end);

/* Stores a sorted list of RAM memory ranges for which to create elf headers.
 * A separate program header is created for backup region */
static struct memory_range crash_memory_range[CRASH_MAX_MEMORY_RANGES];

/* Memory region reserved for storing panic kernel and other data. */
#define CRASH_RESERVED_MEM_NR	8
static struct memory_range crash_reserved_mem[CRASH_RESERVED_MEM_NR];
static int crash_reserved_mem_nr;

/* Reads the appropriate file and retrieves the SYSTEM RAM regions for whom to
 * create Elf headers. Keeping it separate from get_memory_ranges() as
 * requirements are different in the case of normal kexec and crashdumps.
 *
 * Normal kexec needs to look at all of available physical memory irrespective
 * of the fact how much of it is being used by currently running kernel.
 * Crashdumps need to have access to memory regions actually being used by
 * running  kernel. Expecting a different file/data structure than /proc/iomem
 * to look into down the line. May be something like /proc/kernelmem or may
 * be zone data structures exported from kernel.
 */
static int get_crash_memory_ranges(struct memory_range **range, int *ranges,
				   int kexec_flags, unsigned long lowmem_limit)
{
	const char *iomem = proc_iomem();
	int memory_ranges = 0, gart = 0, i;
	char line[MAX_LINE];
	FILE *fp;
	unsigned long long start, end;
	uint64_t gart_start = 0, gart_end = 0;

	fp = fopen(iomem, "r");
	if (!fp) {
		fprintf(stderr, "Cannot open %s: %s\n",
			iomem, strerror(errno));
		return -1;
	}

	while(fgets(line, sizeof(line), fp) != 0) {
		char *str;
		int type, consumed, count;

		if (memory_ranges >= CRASH_MAX_MEMORY_RANGES)
			break;
		count = sscanf(line, "%llx-%llx : %n",
			&start, &end, &consumed);
		if (count != 2)
			continue;
		str = line + consumed;
		dbgprintf("%016llx-%016llx : %s",
			start, end, str);
		/*
		 * We want to dump any System RAM -- memory regions currently
		 * used by the kernel. In the usual case, this is "System RAM"
		 * on the top level. However, we can also have "System RAM
		 * (virtio_mem)" below virtio devices or "System RAM (kmem)"
		 * below "Persistent Memory".
		 */
		if (strstr(str, "System RAM")) {
			type = RANGE_RAM;
		} else if (memcmp(str, "ACPI Tables\n", 12) == 0) {
			/*
			 * ACPI Tables area need to be passed to new
			 * kernel with appropriate memmap= option. This
			 * is needed so that x86_64 kernel creates linear
			 * mapping for this region which is required for
			 * initializing acpi tables in second kernel.
			 */
			type = RANGE_ACPI;
		} else if(memcmp(str,"ACPI Non-volatile Storage\n",26) == 0 ) {
			type = RANGE_ACPI_NVS;
		} else if(memcmp(str,"Persistent Memory (legacy)\n",27) == 0 ) {
			type = RANGE_PRAM;
		} else if(memcmp(str,"Persistent Memory\n",18) == 0 ) {
			type = RANGE_PMEM;
		} else if(memcmp(str,"reserved\n",9) == 0 ) {
			type = RANGE_RESERVED;
		} else if (memcmp(str, "Reserved\n", 9) == 0) {
			type = RANGE_RESERVED;
		} else if (memcmp(str, "GART\n", 5) == 0) {
			gart_start = start;
			gart_end = end;
			gart = 1;
			continue;
		} else {
			continue;
		}

		crash_memory_range[memory_ranges].start = start;
		crash_memory_range[memory_ranges].end = end;
		crash_memory_range[memory_ranges].type = type;

		segregate_lowmem_region(&memory_ranges, lowmem_limit);

		memory_ranges++;
	}
	fclose(fp);
	if (kexec_flags & KEXEC_PRESERVE_CONTEXT) {
		for (i = 0; i < memory_ranges; i++) {
			if (crash_memory_range[i].end > 0x0009ffff) {
				crash_reserved_mem[0].start = \
					crash_memory_range[i].start;
				break;
			}
		}
		if (crash_reserved_mem[0].start >= mem_max) {
			fprintf(stderr, "Too small mem_max: 0x%llx.\n",
				mem_max);
			return -1;
		}
		crash_reserved_mem[0].end = mem_max;
		crash_reserved_mem[0].type = RANGE_RAM;
		crash_reserved_mem_nr = 1;
	}

	for (i = 0; i < crash_reserved_mem_nr; i++)
		if (exclude_region(&memory_ranges, crash_reserved_mem[i].start,
				crash_reserved_mem[i].end) < 0)
			return -1;

	if (gart) {
		/* exclude GART region if the system has one */
		if (exclude_region(&memory_ranges, gart_start, gart_end) < 0)
			return -1;
	}
	*range = crash_memory_range;
	*ranges = memory_ranges;

	return 0;
}

#ifdef HAVE_LIBXENCTRL
static int get_crash_memory_ranges_xen(struct memory_range **range,
					int *ranges, unsigned long lowmem_limit)
{
	struct e820entry *e820entries;
	int j, rc, ret = -1;
	unsigned int i;
	xc_interface *xc;

	xc = xc_interface_open(NULL, NULL, 0);

	if (!xc) {
		fprintf(stderr, "%s: Failed to open Xen control interface\n", __func__);
		return -1;
	}

	e820entries = xmalloc(sizeof(*e820entries) * CRASH_MAX_MEMORY_RANGES);

	rc = xc_get_machine_memory_map(xc, e820entries, CRASH_MAX_MEMORY_RANGES);

	if (rc < 0) {
		fprintf(stderr, "%s: xc_get_machine_memory_map: %s\n", __func__, strerror(-rc));
		goto err;
	}

	for (i = 0, j = 0; i < rc && j < CRASH_MAX_MEMORY_RANGES; ++i, ++j) {
		crash_memory_range[j].start = e820entries[i].addr;
		crash_memory_range[j].end = e820entries[i].addr + e820entries[i].size - 1;
		crash_memory_range[j].type = xen_e820_to_kexec_type(e820entries[i].type);
		segregate_lowmem_region(&j, lowmem_limit);
	}

	*range = crash_memory_range;
	*ranges = j;

	qsort(*range, *ranges, sizeof(struct memory_range), compare_ranges);

	for (i = 0; i < crash_reserved_mem_nr; i++)
		if (exclude_region(ranges, crash_reserved_mem[i].start,
						crash_reserved_mem[i].end) < 0)
			goto err;

	ret = 0;

err:
	xc_interface_close(xc);
	free(e820entries);
	return ret;
}
#else
static int get_crash_memory_ranges_xen(struct memory_range **range,
					int *ranges, unsigned long lowmem_limit)
{
	return 0;
}
#endif /* HAVE_LIBXENCTRL */

static void segregate_lowmem_region(int *nr_ranges, unsigned long lowmem_limit)
{
	unsigned long long end, start;
	unsigned type;

	start = crash_memory_range[*nr_ranges].start;
	end = crash_memory_range[*nr_ranges].end;
	type = crash_memory_range[*nr_ranges].type;

	if (!(lowmem_limit && lowmem_limit > start && lowmem_limit < end))
		return;

	crash_memory_range[*nr_ranges].end = lowmem_limit - 1;

	if (*nr_ranges >= CRASH_MAX_MEMORY_RANGES - 1)
		return;

	++*nr_ranges;

	crash_memory_range[*nr_ranges].start = lowmem_limit;
	crash_memory_range[*nr_ranges].end = end;
	crash_memory_range[*nr_ranges].type = type;
}

/* Removes crash reserve region from list of memory chunks for whom elf program
 * headers have to be created. Assuming crash reserve region to be a single
 * continuous area fully contained inside one of the memory chunks */
static int exclude_region(int *nr_ranges, uint64_t start, uint64_t end)
{
	int i, j, tidx = -1;
	struct memory_range temp_region = {0, 0, 0};


	for (i = 0; i < (*nr_ranges); i++) {
		unsigned long long mstart, mend;
		mstart = crash_memory_range[i].start;
		mend = crash_memory_range[i].end;
		if (start < mend && end > mstart) {
			if (start != mstart && end != mend) {
				/* Split memory region */
				crash_memory_range[i].end = start - 1;
				temp_region.start = end + 1;
				temp_region.end = mend;
				temp_region.type = RANGE_RAM;
				tidx = i+1;
			} else if (start != mstart)
				crash_memory_range[i].end = start - 1;
			else
				crash_memory_range[i].start = end + 1;
		}
	}
	/* Insert split memory region, if any. */
	if (tidx >= 0) {
		if (*nr_ranges == CRASH_MAX_MEMORY_RANGES) {
			/* No space to insert another element. */
			fprintf(stderr, "Error: Number of crash memory ranges"
					" excedeed the max limit\n");
			return -1;
		}
		for (j = (*nr_ranges - 1); j >= tidx; j--)
			crash_memory_range[j+1] = crash_memory_range[j];
		crash_memory_range[tidx] = temp_region;
		(*nr_ranges)++;
	}
	return 0;
}

/* Adds a segment from list of memory regions which new kernel can use to
 * boot. Segment start and end should be aligned to 1K boundary. */
static int add_memmap(struct memory_range *memmap_p, int *nr_memmap,
			unsigned long long addr, size_t size, int type)
{
	int i, j, nr_entries = 0, tidx = 0, align = 1024;
	unsigned long long mstart, mend;

	/* Shrink to 1KiB alignment if needed. */
	if (type == RANGE_RAM && ((addr%align) || (size%align))) {
		unsigned long long end = addr + size;

		printf("%s: RAM chunk %#llx - %#llx unaligned\n", __func__, addr, end);
		addr = _ALIGN_UP(addr, align);
		end = _ALIGN_DOWN(end, align);
		if (addr >= end)
			return -1;
		size = end - addr;
		printf("%s: RAM chunk shrunk to %#llx - %#llx\n", __func__, addr, end);
	}

	/* Make sure at least one entry in list is free. */
	for (i = 0; i < CRASH_MAX_MEMMAP_NR;  i++) {
		mstart = memmap_p[i].start;
		mend = memmap_p[i].end;
		if (!mstart  && !mend)
			break;
		else
			nr_entries++;
	}
	if (nr_entries == CRASH_MAX_MEMMAP_NR)
		return -1;

	for (i = 0; i < CRASH_MAX_MEMMAP_NR;  i++) {
		mstart = memmap_p[i].start;
		mend = memmap_p[i].end;
		if (mstart == 0 && mend == 0)
			break;
		if (mstart <= (addr+size-1) && mend >=addr)
			/* Overlapping region. */
			return -1;
		else if (addr > mend)
			tidx = i+1;
	}
	/* Insert the memory region. */
	for (j = nr_entries-1; j >= tidx; j--)
		memmap_p[j+1] = memmap_p[j];
	memmap_p[tidx].start = addr;
	memmap_p[tidx].end = addr + size - 1;
	memmap_p[tidx].type = type;
	*nr_memmap = nr_entries + 1;

	dbgprint_mem_range("Memmap after adding segment", memmap_p, *nr_memmap);

	return 0;
}

/* Removes a segment from list of memory regions which new kernel can use to
 * boot. Segment start and end should be aligned to 1K boundary. */
static int delete_memmap(struct memory_range *memmap_p, int *nr_memmap,
				unsigned long long addr, size_t size)
{
	int i, j, nr_entries = 0, tidx = -1, operation = 0, align = 1024;
	unsigned long long mstart, mend;
	struct memory_range temp_region;

	/* Do alignment check. */
	if ((addr%align) || (size%align))
		return -1;

	/* Make sure at least one entry in list is free. */
	for (i = 0; i < CRASH_MAX_MEMMAP_NR;  i++) {
		mstart = memmap_p[i].start;
		mend = memmap_p[i].end;
		if (!mstart  && !mend)
			break;
		else
			nr_entries++;
	}
	if (nr_entries == CRASH_MAX_MEMMAP_NR)
		/* List if full */
		return -1;

	for (i = 0; i < CRASH_MAX_MEMMAP_NR;  i++) {
		mstart = memmap_p[i].start;
		mend = memmap_p[i].end;
		if (mstart == 0 && mend == 0)
			/* Did not find the segment in the list. */
			return -1;
		if (mstart <= addr && mend >= (addr + size - 1)) {
			if (mstart == addr && mend == (addr + size - 1)) {
				/* Exact match. Delete region */
				operation = -1;
				tidx = i;
				break;
			}
			if (mstart != addr && mend != (addr + size - 1)) {
				/* Split in two */
				memmap_p[i].end = addr - 1;
				temp_region.start = addr + size;
				temp_region.end = mend;
				temp_region.type = memmap_p[i].type;
				operation = 1;
				tidx = i;
				break;
			}

			/* No addition/deletion required. Adjust the existing.*/
			if (mstart != addr) {
				memmap_p[i].end = addr - 1;
				break;
			} else {
				memmap_p[i].start = addr + size;
				break;
			}
		}
	}
	if ((operation == 1) && tidx >=0) {
		/* Insert the split memory region. */
		for (j = nr_entries-1; j > tidx; j--)
			memmap_p[j+1] = memmap_p[j];
		memmap_p[tidx+1] = temp_region;
		*nr_memmap = nr_entries + 1;
	}
	if ((operation == -1) && tidx >=0) {
		/* Delete the exact match memory region. */
		for (j = i+1; j < CRASH_MAX_MEMMAP_NR; j++)
			memmap_p[j-1] = memmap_p[j];
		memmap_p[j-1].start = memmap_p[j-1].end = 0;
		*nr_memmap = nr_entries - 1;
	}

	dbgprint_mem_range("Memmap after deleting segment", memmap_p, *nr_memmap);

	return 0;
}

static void cmdline_add_memmap_internal(char *cmdline, unsigned long startk,
					unsigned long endk, int type)
{
	int cmdlen, len;
	char str_mmap[256], str_tmp[20];

	strcpy (str_mmap, " memmap=");
	ultoa((endk-startk), str_tmp);
	strcat (str_mmap, str_tmp);

	if (type == RANGE_RAM)
		strcat (str_mmap, "K@");
	else if (type == RANGE_RESERVED)
		strcat (str_mmap, "K$");
	else if (type == RANGE_ACPI || type == RANGE_ACPI_NVS)
		strcat (str_mmap, "K#");
	else if (type == RANGE_PRAM)
		strcat (str_mmap, "K!");

	ultoa(startk, str_tmp);
	strcat (str_mmap, str_tmp);
	strcat (str_mmap, "K");
	len = strlen(str_mmap);
	cmdlen = strlen(cmdline) + len;
	if (cmdlen > (COMMAND_LINE_SIZE - 1))
		die("Command line overflow\n");
	strcat(cmdline, str_mmap);
}

/* Adds the appropriate memmap= options to command line, indicating the
 * memory regions the new kernel can use to boot into. */
static int cmdline_add_memmap(char *cmdline, struct memory_range *memmap_p)
{
	int i, cmdlen, len;
	unsigned long min_sizek = 100;
	char str_mmap[256];

	/* Exact map */
	strcpy(str_mmap, " memmap=exactmap");
	len = strlen(str_mmap);
	cmdlen = strlen(cmdline) + len;
	if (cmdlen > (COMMAND_LINE_SIZE - 1))
		die("Command line overflow\n");
	strcat(cmdline, str_mmap);

	for (i = 0; i < CRASH_MAX_MEMMAP_NR;  i++) {
		unsigned long startk, endk, type;

		startk = memmap_p[i].start/1024;
		endk = (memmap_p[i].end + 1)/1024;
		type = memmap_p[i].type;

		/* Only adding memory regions of RAM and ACPI and Persistent Mem */
		if (type != RANGE_RAM &&
		    type != RANGE_ACPI &&
		    type != RANGE_ACPI_NVS &&
		    type != RANGE_PRAM)
			continue;

		if (type == RANGE_ACPI || type == RANGE_ACPI_NVS)
			endk = _ALIGN_UP(memmap_p[i].end + 1, 1024)/1024;

		if (!startk && !endk)
			/* All regions traversed. */
			break;

		/* A RAM region is not worth adding if region size < 100K.
		 * It eats up precious command line length. */
		if (type == RANGE_RAM && (endk - startk) < min_sizek)
			continue;
		/* And do not add e820 reserved region either */
		cmdline_add_memmap_internal(cmdline, startk, endk, type);
	}

	dbgprintf("Command line after adding memmap\n");
	dbgprintf("%s\n", cmdline);

	return 0;
}

/* Adds the elfcorehdr= command line parameter to command line. */
static int cmdline_add_elfcorehdr(char *cmdline, unsigned long addr)
{
	int cmdlen, len, align = 1024;
	char str[30], *ptr;

	/* Passing in elfcorehdr=xxxK format. Saves space required in cmdline.
	 * Ensure 1K alignment*/
	if (addr%align)
		return -1;
	addr = addr/align;
	ptr = str;
	strcpy(str, " elfcorehdr=");
	ptr += strlen(str);
	ultoa(addr, ptr);
	strcat(str, "K");
	len = strlen(str);
	cmdlen = strlen(cmdline) + len;
	if (cmdlen > (COMMAND_LINE_SIZE - 1))
		die("Command line overflow\n");
	strcat(cmdline, str);

	dbgprintf("Command line after adding elfcorehdr\n");
	dbgprintf("%s\n", cmdline);

	return 0;
}


/*
 * This routine is specific to i386 architecture to maintain the
 * backward compatibility, other architectures can use the per
 * cpu version get_crash_notes_per_cpu() directly.
 */
static int get_crash_notes(int cpu, uint64_t *addr, uint64_t *len)
{
	const char *crash_notes = "/sys/kernel/crash_notes";
	char line[MAX_LINE];
	FILE *fp;
	unsigned long vaddr;
	int count;

	fp = fopen(crash_notes, "r");
	if (fp) {
		if (fgets(line, sizeof(line), fp) != 0) {
			count = sscanf(line, "%lx", &vaddr);
			if (count != 1)
				die("Cannot parse %s: %s\n", crash_notes,
						strerror(errno));
		}

		*addr = x86__pa(vaddr + (cpu * MAX_NOTE_BYTES));
		*len = MAX_NOTE_BYTES;

		dbgprintf("crash_notes addr = %llx\n",
			  (unsigned long long)*addr);

		fclose(fp);
		return 0;
	} else
		return get_crash_notes_per_cpu(cpu, addr, len);
}

static enum coretype get_core_type(struct crash_elf_info *elf_info,
				   struct memory_range *range, int ranges)
{
	if ((elf_info->machine) == EM_X86_64)
		return CORE_TYPE_ELF64;
	else {
		/* fall back to default */
		if (ranges == 0)
			return CORE_TYPE_ELF64;

		if (range[ranges - 1].end > 0xFFFFFFFFUL)
			return CORE_TYPE_ELF64;
		else
			return CORE_TYPE_ELF32;
	}
}

static int sysfs_efi_runtime_map_exist(void)
{
	DIR *dir;

	dir = opendir("/sys/firmware/efi/runtime-map");
	if (!dir)
		return 0;

	closedir(dir);
	return 1;
}

/* Appends 'acpi_rsdp=' commandline for efi boot crash dump */
static void cmdline_add_efi(char *cmdline)
{
	uint64_t acpi_rsdp;
	char acpi_rsdp_buf[MAX_LINE];

	acpi_rsdp = get_acpi_rsdp();

	if (!acpi_rsdp)
		return;

	sprintf(acpi_rsdp_buf, " acpi_rsdp=0x%lx", acpi_rsdp);
	if (strlen(cmdline) + strlen(acpi_rsdp_buf) > (COMMAND_LINE_SIZE - 1))
		die("Command line overflow\n");

	strcat(cmdline, acpi_rsdp_buf);
}

static void get_backup_area(struct kexec_info *info,
				struct memory_range *range, int ranges)
{
	int i;

	/* Look for first 640 KiB RAM region. */
	for (i = 0; i < ranges; ++i) {
		if (range[i].type != RANGE_RAM || range[i].end > 0xa0000)
			continue;

		info->backup_src_start = range[i].start;
		info->backup_src_size = range[i].end - range[i].start + 1;

		dbgprintf("%s: %016llx-%016llx : System RAM\n", __func__,
						range[i].start, range[i].end);

		return;
	}

	/* First 640 KiB RAM region not found. Assume defaults. */
	info->backup_src_start = BACKUP_SRC_START;
	info->backup_src_size = BACKUP_SRC_END - BACKUP_SRC_START + 1;
}

/* Loads additional segments in case of a panic kernel is being loaded.
 * One segment for backup region, another segment for storing elf headers
 * for crash memory image.
 */
int load_crashdump_segments(struct kexec_info *info, char* mod_cmdline,
				unsigned long max_addr, unsigned long min_base)
{
	void *tmp;
	unsigned long sz, bufsz, memsz, elfcorehdr;
	int nr_ranges = 0, nr_memmap = 0, align = 1024, i;
	struct memory_range *mem_range, *memmap_p;
	struct crash_elf_info elf_info;
	unsigned kexec_arch;

	memset(&elf_info, 0x0, sizeof(elf_info));

	/* Constant parts of the elf_info */
	memset(&elf_info, 0, sizeof(elf_info));
	elf_info.data             = ELFDATA2LSB;

	/* Get the architecture of the running kernel */
	kexec_arch = info->kexec_flags & KEXEC_ARCH_MASK;
	if (kexec_arch == KEXEC_ARCH_DEFAULT)
		kexec_arch = KEXEC_ARCH_NATIVE;
	
        /* Get the elf architecture of the running kernel */
	switch(kexec_arch) {
	case KEXEC_ARCH_X86_64:
		elf_info.machine = EM_X86_64;
		break;
	case KEXEC_ARCH_386:
		elf_info.machine       = EM_386;
		elf_info.lowmem_limit  = X86_MAXMEM;
		elf_info.get_note_info = get_crash_notes;
		break;
	default:
		fprintf(stderr, "unsupported crashdump architecture: %04x\n",
			kexec_arch);
		return -1;
	}

	if (xen_present()) {
		if (get_crash_memory_ranges_xen(&mem_range, &nr_ranges,
						elf_info.lowmem_limit) < 0)
			return -1;
	} else
		if (get_crash_memory_ranges(&mem_range, &nr_ranges,
						info->kexec_flags,
						elf_info.lowmem_limit) < 0)
			return -1;

	get_backup_area(info, mem_range, nr_ranges);

	dbgprint_mem_range("CRASH MEMORY RANGES", mem_range, nr_ranges);

	/*
	 * if the core type has not been set on command line, set it here
	 * automatically
	 */
	if (arch_options.core_header_type == CORE_TYPE_UNDEF) {
		arch_options.core_header_type =
			get_core_type(&elf_info, mem_range, nr_ranges);
	}
	/* Get the elf class... */
	elf_info.class = ELFCLASS32;
	if (arch_options.core_header_type == CORE_TYPE_ELF64) {
		elf_info.class = ELFCLASS64;
	}

	if (get_kernel_page_offset(info, &elf_info))
		return -1;

	if (get_kernel_paddr(info, &elf_info))
		return -1;

	if (get_kernel_vaddr_and_size(info, &elf_info))
		return -1;

	/* Memory regions which panic kernel can safely use to boot into */
	sz = (sizeof(struct memory_range) * CRASH_MAX_MEMMAP_NR);
	memmap_p = xmalloc(sz);
	memset(memmap_p, 0, sz);
	add_memmap(memmap_p, &nr_memmap, info->backup_src_start, info->backup_src_size, RANGE_RAM);
	for (i = 0; i < crash_reserved_mem_nr; i++) {
		sz = crash_reserved_mem[i].end - crash_reserved_mem[i].start +1;
		if (add_memmap(memmap_p, &nr_memmap, crash_reserved_mem[i].start,
				sz, RANGE_RAM) < 0) {
			free(memmap_p);
			return ENOCRASHKERNEL;
		}
	}

	/* Create a backup region segment to store backup data*/
	if (!(info->kexec_flags & KEXEC_PRESERVE_CONTEXT)) {
		sz = _ALIGN(info->backup_src_size, align);
		tmp = xmalloc(sz);
		memset(tmp, 0, sz);
		info->backup_start = add_buffer(info, tmp, sz, sz, align,
						0, max_addr, -1);
		dbgprintf("Created backup segment at 0x%lx\n",
			  info->backup_start);
		if (delete_memmap(memmap_p, &nr_memmap, info->backup_start, sz) < 0) {
			free(tmp);
			free(memmap_p);
			return EFAILED;
		}
	}

	/* Create elf header segment and store crash image data. */
	if (arch_options.core_header_type == CORE_TYPE_ELF64) {
		if (crash_create_elf64_headers(info, &elf_info, mem_range,
						nr_ranges, &tmp, &bufsz,
						ELF_CORE_HEADER_ALIGN) < 0) {
			free(memmap_p);
			return EFAILED;
		}
	}
	else {
		if (crash_create_elf32_headers(info, &elf_info, mem_range,
						nr_ranges, &tmp, &bufsz,
						ELF_CORE_HEADER_ALIGN) < 0) {
			free(memmap_p);
			return EFAILED;
		}
	}
	/* the size of the elf headers allocated is returned in 'bufsz' */

	/* Hack: With some ld versions (GNU ld version 2.14.90.0.4 20030523),
	 * vmlinux program headers show a gap of two pages between bss segment
	 * and data segment but effectively kernel considers it as bss segment
	 * and overwrites the any data placed there. Hence bloat the memsz of
	 * elf core header segment to 16K to avoid being placed in such gaps.
	 * This is a makeshift solution until it is fixed in kernel.
	 */
	if (bufsz < (16*1024)) {
		/* bufsize is big enough for all the PT_NOTE's and PT_LOAD's */
		memsz = 16*1024;
		/* memsz will be the size of the memory hole we look for */
	} else {
		memsz = bufsz;
	}

	/* For hotplug support, override the minimum necessary size just
	 * computed with the value from /sys/kernel/crash_elfcorehdr_size.
	 * Properly align the size as well.
	 */
	if (do_hotplug) {
		memsz = _ALIGN(elfcorehdrsz, align);
	}

	/* Record the location of the elfcorehdr for hotplug handling */
	info->elfcorehdr =
	elfcorehdr = add_buffer(info, tmp, bufsz, memsz, align, min_base,
							max_addr, -1);
	dbgprintf("Created elf header segment at 0x%lx\n", elfcorehdr);
	if (delete_memmap(memmap_p, &nr_memmap, elfcorehdr, memsz) < 0) {
		free(memmap_p);
		return -1;
		}
	if (!bzImage_support_efi_boot || arch_options.noefi ||
	    !sysfs_efi_runtime_map_exist())
		cmdline_add_efi(mod_cmdline);
	cmdline_add_elfcorehdr(mod_cmdline, elfcorehdr);

	/* Inform second kernel about the presence of ACPI tables. */
	for (i = 0; i < nr_ranges; i++) {
		unsigned long start, end, size, type;
		if ( !( mem_range[i].type == RANGE_ACPI
			|| mem_range[i].type == RANGE_ACPI_NVS
			|| mem_range[i].type == RANGE_RESERVED
			|| mem_range[i].type == RANGE_PMEM
			|| mem_range[i].type == RANGE_PRAM))
			continue;
		start = mem_range[i].start;
		end = mem_range[i].end;
		type = mem_range[i].type;
		size = end - start + 1;
		add_memmap(memmap_p, &nr_memmap, start, size, type);
	}

	if (arch_options.pass_memmap_cmdline)
		cmdline_add_memmap(mod_cmdline, memmap_p);

	/* Store 2nd kernel boot memory ranges for later reference in
	 * x86-setup-linux.c: setup_linux_system_parameters() */
	info->crash_range = memmap_p;
	info->nr_crash_ranges = nr_memmap;

	return 0;
}

/* On x86, the kernel may make a low reservation in addition to the
 * normal reservation. However, the kernel refuses to load the panic
 * kernel to low memory, so always choose the highest range.
 */
int get_crash_kernel_load_range(uint64_t *start, uint64_t *end)
{
	if (!crash_reserved_mem_nr)
		return -1;

	*start = crash_reserved_mem[crash_reserved_mem_nr - 1].start;
	*end = crash_reserved_mem[crash_reserved_mem_nr - 1].end;

	return 0;
}

static int crashkernel_mem_callback(void *UNUSED(data), int nr,
                                          char *UNUSED(str),
                                          unsigned long long base,
                                          unsigned long long length)
{
	if (nr >= CRASH_RESERVED_MEM_NR)
		return 1;

	crash_reserved_mem[nr].start = base;
	crash_reserved_mem[nr].end   = base + length - 1;
	crash_reserved_mem[nr].type  = RANGE_RAM;
	return 0;
}

int is_crashkernel_mem_reserved(void)
{
	int ret;

	if (xen_present()) {
		uint64_t start, end;

		ret = xen_get_crashkernel_region(&start, &end);
		if (ret < 0)
			return 0;

		crash_reserved_mem[0].start = start;
		crash_reserved_mem[0].end   = end;
		crash_reserved_mem[0].type  = RANGE_RAM;
		crash_reserved_mem_nr = 1;
	} else {
		ret = kexec_iomem_for_each_line("Crash kernel\n",
						crashkernel_mem_callback, NULL);
		crash_reserved_mem_nr = ret;
	}

	return !!crash_reserved_mem_nr;
}
