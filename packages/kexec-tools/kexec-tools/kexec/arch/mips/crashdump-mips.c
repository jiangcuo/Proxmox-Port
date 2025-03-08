/*
 * kexec: Linux boots Linux
 *
 * 2005 (C) IBM Corporation.
 * 2008 (C) MontaVista Software, Inc.
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
#include <elf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"
#include "../../kexec-syscall.h"
#include "../../crashdump.h"
#include "kexec-mips.h"
#include "crashdump-mips.h"
#include "unused.h"

/* Stores a sorted list of RAM memory ranges for which to create elf headers.
 * A separate program header is created for backup region */
static struct memory_range crash_memory_range[CRASH_MAX_MEMORY_RANGES];

/* Not used currently but required by generic fs2dt code */
struct memory_ranges usablemem_rgns;

/* Memory region reserved for storing panic kernel and other data. */
static struct memory_range crash_reserved_mem;

/* Read kernel physical load addr from the file returned by proc_iomem()
 * (Kernel Code) and store in kexec_info */
static int get_kernel_paddr(struct crash_elf_info *elf_info)
{
	uint64_t start;

	if (xen_present()) /* Kernel not entity mapped under Xen */
		return 0;

	if (parse_iomem_single("Kernel code\n", &start, NULL) == 0) {
		elf_info->kern_paddr_start = start;
		dbgprintf("kernel load physical addr start = 0x%" PRIu64 "\n", start);
		return 0;
	}

	fprintf(stderr, "Cannot determine kernel physical load addr\n");
	return -1;
}

static int get_kernel_vaddr_and_size(struct crash_elf_info *elf_info,
				     unsigned long start_offset)
{
	uint64_t end;

	if (!elf_info->kern_paddr_start)
		return -1;

	elf_info->kern_vaddr_start = elf_info->kern_paddr_start |
					start_offset;
	/* If "Kernel bss" exists, the kernel ends there, else fall
	 *  through and say that it ends at "Kernel data" */
	if (parse_iomem_single("Kernel bss\n", NULL, &end) == 0 ||
	    parse_iomem_single("Kernel data\n", NULL, &end) == 0) {
		elf_info->kern_size = end - elf_info->kern_paddr_start;
		dbgprintf("kernel_vaddr= 0x%llx paddr %llx\n",
				elf_info->kern_vaddr_start,
				elf_info->kern_paddr_start);
		dbgprintf("kernel size = 0x%lx\n", elf_info->kern_size);
		return 0;
		}
	fprintf(stderr, "Cannot determine kernel virtual load addr and  size\n");
	return -1;
}

/* Removes crash reserve region from list of memory chunks for whom elf program
 * headers have to be created. Assuming crash reserve region to be a single
 * continuous area fully contained inside one of the memory chunks */
static int exclude_crash_reserve_region(int *nr_ranges)
{
	int i, j, tidx = -1;
	unsigned long long cstart, cend;
	struct memory_range temp_region = {
		.start = 0,
		.end = 0
	};

	/* Crash reserved region. */
	cstart = crash_reserved_mem.start;
	cend = crash_reserved_mem.end;

	for (i = 0; i < (*nr_ranges); i++) {
		unsigned long long mstart, mend;
		mstart = crash_memory_range[i].start;
		mend = crash_memory_range[i].end;
		if (cstart < mend && cend > mstart) {
			if (cstart != mstart && cend != mend) {
				/* Split memory region */
				crash_memory_range[i].end = cstart - 1;
				temp_region.start = cend + 1;
				temp_region.end = mend;
				temp_region.type = RANGE_RAM;
				tidx = i+1;
			} else if (cstart != mstart)
				crash_memory_range[i].end = cstart - 1;
			else
				crash_memory_range[i].start = cend + 1;
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
		crash_memory_range[tidx].start = temp_region.start;
		crash_memory_range[tidx].end = temp_region.end;
		crash_memory_range[tidx].type = temp_region.type;
		(*nr_ranges)++;
	}
	return 0;
}
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
static int get_crash_memory_ranges(struct memory_range **range, int *ranges)
{
	const char iomem[] = "/proc/iomem";
	int memory_ranges = 0;
	char line[MAX_LINE];
	FILE *fp;
	unsigned long long start, end;

	fp = fopen(iomem, "r");
	if (!fp) {
		fprintf(stderr, "Cannot open %s: %s\n",
			iomem, strerror(errno));
		return -1;
	}
	/* Separate segment for backup region */
	crash_memory_range[0].start = BACKUP_SRC_START;
	crash_memory_range[0].end = BACKUP_SRC_END;
	crash_memory_range[0].type = RANGE_RAM;
	memory_ranges++;

	while (fgets(line, sizeof(line), fp) != 0) {
		char *str;
		int type, consumed, count;
		if (memory_ranges >= CRASH_MAX_MEMORY_RANGES)
			break;
		count = sscanf(line, "%llx-%llx : %n",
			&start, &end, &consumed);
		if (count != 2)
			continue;
		str = line + consumed;

		/* Only Dumping memory of type System RAM. */
		if (memcmp(str, "System RAM\n", 11) == 0) {
			type = RANGE_RAM;
		} else if (memcmp(str, "Crash kernel\n", 13) == 0) {
				/* Reserved memory region. New kernel can
				 * use this region to boot into. */
				crash_reserved_mem.start = start;
				crash_reserved_mem.end = end;
				crash_reserved_mem.type = RANGE_RAM;
				continue;
		} else
			continue;

		if (start == BACKUP_SRC_START && end >= (BACKUP_SRC_END + 1))
			start = BACKUP_SRC_END + 1;

		crash_memory_range[memory_ranges].start = start;
		crash_memory_range[memory_ranges].end = end;
		crash_memory_range[memory_ranges].type = type;
		memory_ranges++;

		/* Segregate linearly mapped region. */
		if (MAXMEM && (MAXMEM - 1) >= start && (MAXMEM - 1) <= end) {
			crash_memory_range[memory_ranges - 1].end = MAXMEM - 1;

			/* Add segregated region. */
			crash_memory_range[memory_ranges].start = MAXMEM;
			crash_memory_range[memory_ranges].end = end;
			crash_memory_range[memory_ranges].type = type;
			memory_ranges++;
		}
	}
	fclose(fp);

	if (exclude_crash_reserve_region(&memory_ranges) < 0)
		return -1;

	*range = crash_memory_range;
	*ranges = memory_ranges;
	return 0;
}

/* Adds the appropriate mem= options to command line, indicating the
 * memory region the new kernel can use to boot into. */
static int cmdline_add_mem(char *cmdline, unsigned long addr,
		unsigned long size)
{
	int cmdlen, len;
	char str[50], *ptr;

	addr = addr/1024;
	size = size/1024;
	ptr = str;
	strcpy(str, " mem=");
	ptr += strlen(str);
	ultoa(size, ptr);
	strcat(str, "K@");
	ptr = str + strlen(str);
	ultoa(addr, ptr);
	strcat(str, "K");
	len = strlen(str);
	cmdlen = strlen(cmdline) + len;
	if (cmdlen > (COMMAND_LINE_SIZE - 1))
		die("Command line overflow\n");
	strcat(cmdline, str);

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
	return 0;
}

#if __BYTE_ORDER == __LITTLE_ENDIAN
# define ELFDATALOCAL ELFDATA2LSB
#elif __BYTE_ORDER == __BIG_ENDIAN
# define ELFDATALOCAL ELFDATA2MSB
#else
# error Unknown byte order
#endif

static struct crash_elf_info elf_info64 = {
	class: ELFCLASS64,
	data : ELFDATALOCAL,
	machine : EM_MIPS,
	page_offset : PAGE_OFFSET,
	lowmem_limit : 0, /* 0 == no limit */
};

static struct crash_elf_info elf_info32 = {
	class: ELFCLASS32,
	data : ELFDATALOCAL,
	machine : EM_MIPS,
	page_offset : PAGE_OFFSET,
	lowmem_limit : MAXMEM,
};

static int patch_elf_info(void)
{
	const char cpuinfo[] = "/proc/cpuinfo";
	char line[MAX_LINE];
	FILE *fp;

	fp = fopen(cpuinfo, "r");
	if (!fp) {
		fprintf(stderr, "Cannot open %s: %s\n",
			cpuinfo, strerror(errno));
		return -1;
	}
	while (fgets(line, sizeof(line), fp) != 0) {
		if (strncmp(line, "cpu model", 9) == 0) {
			/* OCTEON uses a different page_offset. */
			if (strstr(line, "Octeon"))
				elf_info64.page_offset = OCTEON_PAGE_OFFSET;
			/* LOONGSON uses a different page_offset. */
			else if (strstr(line, "Loongson"))
				elf_info64.page_offset = LOONGSON_PAGE_OFFSET;
			break;
		}
	}
	fclose(fp);
	return 0;
}

/* Loads additional segments in case of a panic kernel is being loaded.
 * One segment for backup region, another segment for storing elf headers
 * for crash memory image.
 */
int load_crashdump_segments(struct kexec_info *info, char* mod_cmdline,
				unsigned long UNUSED(max_addr),
				unsigned long UNUSED(min_base))
{
	void *tmp;
	unsigned long sz, elfcorehdr;
	int nr_ranges, align = 1024;
	struct memory_range *mem_range;
	crash_create_elf_headers_func crash_create = crash_create_elf32_headers;
	struct crash_elf_info *elf_info = &elf_info32;
	unsigned long start_offset = 0x80000000UL;

	if (patch_elf_info())
		return -1;

	if (arch_options.core_header_type == CORE_TYPE_ELF64) {
		elf_info = &elf_info64;
		crash_create = crash_create_elf64_headers;
		start_offset = (unsigned long)0xffffffff80000000UL;
	}

	if (get_kernel_paddr(elf_info))
		return -1;

	if (get_kernel_vaddr_and_size(elf_info, start_offset))
		return -1;

	if (get_crash_memory_ranges(&mem_range, &nr_ranges) < 0)
		return -1;

	info->backup_src_start = BACKUP_SRC_START;
	info->backup_src_size = BACKUP_SRC_SIZE;
	/* Create a backup region segment to store backup data*/
	sz = _ALIGN(BACKUP_SRC_SIZE, align);
	tmp = xmalloc(sz);
	memset(tmp, 0, sz);
	info->backup_start = add_buffer(info, tmp, sz, sz, align,
				crash_reserved_mem.start,
				crash_reserved_mem.end, -1);

	if (crash_create(info, elf_info, crash_memory_range, nr_ranges,
			 &tmp, &sz, ELF_CORE_HEADER_ALIGN) < 0) {
		free(tmp);
		return -1;
	}

	elfcorehdr = add_buffer(info, tmp, sz, sz, align,
		crash_reserved_mem.start,
		crash_reserved_mem.end, -1);

	/*
	 * backup segment is after elfcorehdr, so use elfcorehdr as top of
	 * kernel's available memory
	 */
	cmdline_add_mem(mod_cmdline, crash_reserved_mem.start,
		crash_reserved_mem.end - crash_reserved_mem.start + 1);
	cmdline_add_elfcorehdr(mod_cmdline, elfcorehdr);

	dbgprintf("CRASH MEMORY RANGES:\n");
	dbgprintf("%016Lx-%016Lx\n", crash_reserved_mem.start,
			crash_reserved_mem.end);
	return 0;
}

int is_crashkernel_mem_reserved(void)
{
	uint64_t start, end;

	return parse_iomem_single("Crash kernel\n", &start, &end) == 0 ?
		(start != end) : 0;
}

int get_crash_kernel_load_range(uint64_t *start, uint64_t *end)
{
	return parse_iomem_single("Crash kernel\n", start, end);
}
