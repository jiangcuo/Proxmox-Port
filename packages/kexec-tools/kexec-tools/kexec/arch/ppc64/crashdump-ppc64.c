/*
 * kexec: Linux boots Linux
 *
 * Created by: R Sharada (sharada@in.ibm.com)
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <elf.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"
#include "../../kexec-syscall.h"
#include "../../crashdump.h"
#include "kexec-ppc64.h"
#include "../../fs2dt.h"
#include "crashdump-ppc64.h"

#define DEVTREE_CRASHKERNEL_BASE "/proc/device-tree/chosen/linux,crashkernel-base"
#define DEVTREE_CRASHKERNEL_SIZE "/proc/device-tree/chosen/linux,crashkernel-size"

unsigned int num_of_lmb_sets;
unsigned int is_dyn_mem_v2;
uint64_t lmb_size;

static struct crash_elf_info elf_info64 =
{
	class: ELFCLASS64,
#if BYTE_ORDER == LITTLE_ENDIAN
	data: ELFDATA2LSB,
#else
	data: ELFDATA2MSB,
#endif
	machine: EM_PPC64,
	page_offset: PAGE_OFFSET,
	lowmem_limit: MAXMEM,
};

static struct crash_elf_info elf_info32 =
{
	class: ELFCLASS32,
	data: ELFDATA2MSB,
	machine: EM_PPC64,
	page_offset: PAGE_OFFSET,
	lowmem_limit: MAXMEM,
};

extern struct arch_options_t arch_options;

/* Stores a sorted list of RAM memory ranges for which to create elf headers.
 * A separate program header is created for backup region
 */
static struct memory_range *crash_memory_range = NULL;

/* Define a variable to replace the CRASH_MAX_MEMORY_RANGES macro */
static int crash_max_memory_ranges;

/*
 * Used to save various memory ranges/regions needed for the captured
 * kernel to boot. (lime memmap= option in other archs)
 */
mem_rgns_t usablemem_rgns = {0, NULL};

static unsigned long long cstart, cend;
static int memory_ranges;

/*
 * Exclude the region that lies within crashkernel and above the memory
 * limit which is reflected by mem= kernel option.
 */
static void exclude_crash_region(uint64_t start, uint64_t end)
{
	/* If memory_limit is set then exclude the memory region above it. */
	if (memory_limit) {
		if (start >= memory_limit)
			return;
		if (end > memory_limit)
			end = memory_limit;
	}

	if (cstart < end && cend > start) {
		if (start < cstart && end > cend) {
			crash_memory_range[memory_ranges].start = start;
			crash_memory_range[memory_ranges].end = cstart;
			crash_memory_range[memory_ranges].type = RANGE_RAM;
			memory_ranges++;
			crash_memory_range[memory_ranges].start = cend;
			crash_memory_range[memory_ranges].end = end;
			crash_memory_range[memory_ranges].type = RANGE_RAM;
			memory_ranges++;
		} else if (start < cstart) {
			crash_memory_range[memory_ranges].start = start;
			crash_memory_range[memory_ranges].end = cstart;
			crash_memory_range[memory_ranges].type = RANGE_RAM;
			memory_ranges++;
		} else if (end > cend) {
			crash_memory_range[memory_ranges].start = cend;
			crash_memory_range[memory_ranges].end = end;
			crash_memory_range[memory_ranges].type = RANGE_RAM;
			memory_ranges++;
		}
	} else {
		crash_memory_range[memory_ranges].start = start;
		crash_memory_range[memory_ranges].end  = end;
		crash_memory_range[memory_ranges].type = RANGE_RAM;
		memory_ranges++;
	}
}

static int get_dyn_reconf_crash_memory_ranges(void)
{
	uint64_t start, end;
	uint64_t startrange, endrange;
	uint64_t size;
	char fname[128], buf[32];
	FILE *file;
	unsigned int i;
	int n;
	uint32_t flags;

	strcpy(fname, "/proc/device-tree/");
	strcat(fname, "ibm,dynamic-reconfiguration-memory/ibm,dynamic-memory");
	if (is_dyn_mem_v2)
		strcat(fname, "-v2");
	if ((file = fopen(fname, "r")) == NULL) {
		perror(fname);
		return -1;
	}

	fseek(file, 4, SEEK_SET);
	startrange = endrange = 0;
	size = lmb_size;
	for (i = 0; i < num_of_lmb_sets; i++) {
		if ((n = fread(buf, 1, LMB_ENTRY_SIZE, file)) < 0) {
			perror(fname);
			fclose(file);
			return -1;
		}
		if (memory_ranges >= (max_memory_ranges + 1)) {
			/* No space to insert another element. */
				fprintf(stderr,
				"Error: Number of crash memory ranges"
				" excedeed the max limit\n");
			fclose(file);
			return -1;
		}

		/*
		 * If the property is ibm,dynamic-memory-v2, the first 4 bytes
		 * tell the number of sequential LMBs in this entry.
		 */
		if (is_dyn_mem_v2)
			size = be32_to_cpu(((unsigned int *)buf)[0]) * lmb_size;

		start = be64_to_cpu(*((uint64_t *)&buf[DRCONF_ADDR]));
		end = start + size;
		if (start == 0 && end >= (BACKUP_SRC_END + 1))
			start = BACKUP_SRC_END + 1;

		flags = be32_to_cpu((*((uint32_t *)&buf[DRCONF_FLAGS])));
		/* skip this block if the reserved bit is set in flags (0x80)
		   or if the block is not assigned to this partition (0x8) */
		if ((flags & 0x80) || !(flags & 0x8))
			continue;

		if (start != endrange) {
			if (startrange != endrange)
				exclude_crash_region(startrange, endrange);
			startrange = start;
		}
		endrange = end;
	}
	if (startrange != endrange)
		exclude_crash_region(startrange, endrange);

	fclose(file);
	return 0;
}

/*
 * For a given memory node, check if it is mapped to system RAM or
 * to onboard memory on accelerator device like GPU card or such.
 */
static int is_coherent_device_mem(const char *fname)
{
	char fpath[PATH_LEN];
	char buf[32];
	DIR *dmem;
	FILE *file;
	struct dirent *mentry;
	int cnt, ret = 0;

	strcpy(fpath, fname);
	if ((dmem = opendir(fpath)) == NULL) {
		perror(fpath);
		return -1;
	}

	while ((mentry = readdir(dmem)) != NULL) {
		if (strcmp(mentry->d_name, "compatible"))
			continue;

		strcat(fpath, "/compatible");
		if ((file = fopen(fpath, "r")) == NULL) {
			perror(fpath);
			ret = -1;
			break;
		}
		if ((cnt = fread(buf, 1, 32, file)) < 0) {
			perror(fpath);
			fclose(file);
			ret = -1;
			break;
		}
		if (!strncmp(buf, "ibm,coherent-device-memory", 26)) {
			fclose(file);
			ret = 1;
			break;
		}
		fclose(file);
	}

	closedir(dmem);
	return ret;
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

	char device_tree[256] = "/proc/device-tree/";
	char fname[PATH_LEN];
	char buf[MAXBYTES];
	DIR *dir, *dmem;
	FILE *file;
	struct dirent *dentry, *mentry;
	int n, ret, crash_rng_len = 0;
	unsigned long long start, end;
	int page_size;

	crash_max_memory_ranges = max_memory_ranges + 6;
	crash_rng_len = sizeof(struct memory_range) * crash_max_memory_ranges;

	crash_memory_range = (struct memory_range *) malloc(crash_rng_len);
	if (!crash_memory_range) {
		fprintf(stderr, "Allocation for crash memory range failed\n");
		return -1;
	}
	memset(crash_memory_range, 0, crash_rng_len);

	/* create a separate program header for the backup region */
	crash_memory_range[0].start = BACKUP_SRC_START;
	crash_memory_range[0].end = BACKUP_SRC_END + 1;
	crash_memory_range[0].type = RANGE_RAM;
	memory_ranges++;

	if ((dir = opendir(device_tree)) == NULL) {
		perror(device_tree);
		goto err;
	}

	cstart = crash_base;
	cend = crash_base + crash_size;

	while ((dentry = readdir(dir)) != NULL) {
		if (!strncmp(dentry->d_name,
				"ibm,dynamic-reconfiguration-memory", 35)){
			get_dyn_reconf_crash_memory_ranges();
			continue;
		}
		if (strncmp(dentry->d_name, "memory@", 7) &&
			strcmp(dentry->d_name, "memory"))
			continue;
		strcpy(fname, device_tree);
		strcat(fname, dentry->d_name);

		ret = is_coherent_device_mem(fname);
		if (ret == -1) {
			closedir(dir);
			goto err;
		} else if (ret == 1) {
			/*
			 * Avoid adding this memory region as it is not
			 * mapped to system RAM.
			 */
			continue;
		}

		if ((dmem = opendir(fname)) == NULL) {
			perror(fname);
			closedir(dir);
			goto err;
		}
		while ((mentry = readdir(dmem)) != NULL) {
			if (strcmp(mentry->d_name, "reg"))
				continue;
			strcat(fname, "/reg");
			if ((file = fopen(fname, "r")) == NULL) {
				perror(fname);
				closedir(dmem);
				closedir(dir);
				goto err;
			}
			if ((n = fread(buf, 1, MAXBYTES, file)) < 0) {
				perror(fname);
				fclose(file);
				closedir(dmem);
				closedir(dir);
				goto err;
			}
			if (memory_ranges >= (max_memory_ranges + 1)) {
				/* No space to insert another element. */
				fprintf(stderr,
					"Error: Number of crash memory ranges"
					" excedeed the max limit\n");
				goto err;
			}

			start = be64_to_cpu(((unsigned long long *)buf)[0]);
			end = start +
				be64_to_cpu(((unsigned long long *)buf)[1]);
			if (start == 0 && end >= (BACKUP_SRC_END + 1))
				start = BACKUP_SRC_END + 1;

			exclude_crash_region(start, end);
			fclose(file);
		}
		closedir(dmem);
	}
	closedir(dir);

	/*
	 * If RTAS region is overlapped with crashkernel, need to create ELF
	 * Program header for the overlapped memory.
	 */
	if (crash_base < rtas_base + rtas_size &&
		rtas_base < crash_base + crash_size) {
		page_size = getpagesize();
		cstart = rtas_base;
		cend = rtas_base + rtas_size;
		if (cstart < crash_base)
			cstart = crash_base;
		if (cend > crash_base + crash_size)
			cend = crash_base + crash_size;
		/*
		 * The rtas section created here is formed by reading rtas-base
		 * and rtas-size from /proc/device-tree/rtas.  Unfortunately
		 * rtas-size is not required to be a multiple of PAGE_SIZE
		 * The remainder of the page it ends on is just garbage, and is
		 * safe to read, its just not accounted in rtas-size.  Since
		 * we're creating an elf section here though, lets round it up
		 * to the next page size boundary though, so makedumpfile can
		 * read it safely without going south on us.
		 */
		cend = _ALIGN(cend, page_size);

		crash_memory_range[memory_ranges].start = cstart;
		crash_memory_range[memory_ranges++].end = cend;
	}

	/*
	 * If OPAL region is overlapped with crashkernel, need to create ELF
	 * Program header for the overlapped memory.
	 */
	if (crash_base < opal_base + opal_size &&
		opal_base < crash_base + crash_size) {
		page_size = getpagesize();
		cstart = opal_base;
		cend = opal_base + opal_size;
		if (cstart < crash_base)
			cstart = crash_base;
		if (cend > crash_base + crash_size)
			cend = crash_base + crash_size;
		/*
		 * The opal section created here is formed by reading opal-base
		 * and opal-size from /proc/device-tree/ibm,opal.  Unfortunately
		 * opal-size is not required to be a multiple of PAGE_SIZE
		 * The remainder of the page it ends on is just garbage, and is
		 * safe to read, its just not accounted in opal-size.  Since
		 * we're creating an elf section here though, lets round it up
		 * to the next page size boundary though, so makedumpfile can
		 * read it safely without going south on us.
		 */
		cend = _ALIGN(cend, page_size);

		crash_memory_range[memory_ranges].start = cstart;
		crash_memory_range[memory_ranges++].end = cend;
	}
	*range = crash_memory_range;
	*ranges = memory_ranges;

	int j;
	dbgprintf("CRASH MEMORY RANGES\n");
	for(j = 0; j < *ranges; j++) {
		start = crash_memory_range[j].start;
		end = crash_memory_range[j].end;
		dbgprintf("%016Lx-%016Lx\n", start, end);
	}

	return 0;

err:
	if (crash_memory_range)
		free(crash_memory_range);
	return -1;
}

static int add_cmdline_param(char *cmdline, uint64_t addr, char *cmdstr,
				char *byte)
{
	int cmdline_size, cmdlen, len, align = 1024;
	char str[COMMAND_LINE_SIZE], *ptr;

	/* Passing in =xxxK / =xxxM format. Saves space required in cmdline.*/
	switch (byte[0]) {
		case 'K':
			if (addr%align)
				return -1;
			addr = addr/align;
			break;
		case 'M':
			addr = addr/(align *align);
			break;
	}
	ptr = str;
	strcpy(str, cmdstr);
	ptr += strlen(str);
	ultoa(addr, ptr);
	strcat(str, byte);
	len = strlen(str);
	cmdlen = strlen(cmdline) + len;
	cmdline_size = COMMAND_LINE_SIZE;
	if (cmdlen > (cmdline_size - 1))
		die("Command line overflow\n");
	strcat(cmdline, str);
	dbgprintf("Command line after adding elfcorehdr: %s\n", cmdline);
	return 0;
}

/* Loads additional segments in case of a panic kernel is being loaded.
 * One segment for backup region, another segment for storing elf headers
 * for crash memory image.
 */
int load_crashdump_segments(struct kexec_info *info, char* mod_cmdline,
				uint64_t max_addr, unsigned long min_base)
{
	void *tmp;
	unsigned long sz, memsz;
	uint64_t elfcorehdr;
	int nr_ranges, align = 1024, i;
	unsigned long long end;
	struct memory_range *mem_range;

	if (get_crash_memory_ranges(&mem_range, &nr_ranges) < 0)
		return -1;

	info->backup_src_start = BACKUP_SRC_START;
	info->backup_src_size = BACKUP_SRC_SIZE;
	/* Create a backup region segment to store backup data*/
	sz = _ALIGN(BACKUP_SRC_SIZE, align);
	tmp = xmalloc(sz);
	memset(tmp, 0, sz);
	info->backup_start = add_buffer(info, tmp, sz, sz, align,
					0, max_addr, 1);
	reserve(info->backup_start, sz);

	/* On ppc64 memory ranges in device-tree is denoted as start
	 * and size rather than start and end, as is the case with
	 * other architectures like i386 . Because of this when loading
	 * the memory ranges in crashdump-elf.c the filesz calculation
	 * [ end - start + 1 ] goes for a toss.
	 *
	 * To be in sync with other archs adjust the end value for
	 * every crash memory range before calling the generic function
	 */

	for (i = 0; i < nr_ranges; i++) {
		end = crash_memory_range[i].end - 1;
		crash_memory_range[i].end = end;
	}


	/* Create elf header segment and store crash image data. */
	if (arch_options.core_header_type == CORE_TYPE_ELF64) {
		if (crash_create_elf64_headers(info, &elf_info64,
					       crash_memory_range, nr_ranges,
					       &tmp, &sz,
					       ELF_CORE_HEADER_ALIGN) < 0) {
			free (tmp);
			return -1;
		}
	}
	else {
		if (crash_create_elf32_headers(info, &elf_info32,
					       crash_memory_range, nr_ranges,
					       &tmp, &sz,
					       ELF_CORE_HEADER_ALIGN) < 0) {
			free(tmp);
			return -1;
		}
	}

	memsz = sz;
	/* To support --hotplug, replace the calculated memsz with the value
	 * from /sys/kernel/crash_elfcorehdr_size and align it correctly.
	 */
	if (do_hotplug) {
		if (elfcorehdrsz > sz)
			memsz = _ALIGN(elfcorehdrsz, align);
	}

	/* Record the location of the elfcorehdr for hotplug handling */
	info->elfcorehdr = elfcorehdr = add_buffer(info, tmp, sz, memsz, align,
						   min_base, max_addr, 1);
	reserve(elfcorehdr, sz);
	/* modify and store the cmdline in a global array. This is later
	 * read by flatten_device_tree and modified if required
	 */
	add_cmdline_param(mod_cmdline, elfcorehdr, " elfcorehdr=", "K");
	return 0;
}

/*
 * Used to save various memory regions needed for the captured kernel.
 */

void add_usable_mem_rgns(unsigned long long base, unsigned long long size)
{
	unsigned int i;
	unsigned long long end = base + size;
	unsigned long long ustart, uend;

	base = _ALIGN_DOWN(base, getpagesize());
	end = _ALIGN_UP(end, getpagesize());

	for (i=0; i < usablemem_rgns.size; i++) {
		ustart = usablemem_rgns.ranges[i].start;
		uend = usablemem_rgns.ranges[i].end;
		if (base < uend && end > ustart) {
			if ((base >= ustart) && (end <= uend))
				return;
			if (base < ustart && end > uend) {
				usablemem_rgns.ranges[i].start = base;
				usablemem_rgns.ranges[i].end = end;
#ifdef DEBUG
				fprintf(stderr, "usable memory rgn %u: new base:%llx new size:%llx\n",
					i, base, size);
#endif
				return;
			} else if (base < ustart) {
				usablemem_rgns.ranges[i].start = base;
#ifdef DEBUG
				fprintf(stderr, "usable memory rgn %u: new base:%llx new size:%llx",
					i, base, usablemem_rgns.ranges[i].end - base);
#endif
				return;
			} else if (end > uend){
				usablemem_rgns.ranges[i].end = end;
#ifdef DEBUG
				fprintf(stderr, "usable memory rgn %u: new end:%llx, new size:%llx",
					i, end, end - usablemem_rgns.ranges[i].start);
#endif
				return;
			}
		}
	}
	usablemem_rgns.ranges[usablemem_rgns.size].start = base;
	usablemem_rgns.ranges[usablemem_rgns.size++].end = end;

	dbgprintf("usable memory rgns size:%u base:%llx size:%llx\n",
		usablemem_rgns.size, base, size);
}

int get_crash_kernel_load_range(uint64_t *start, uint64_t *end)
{
	unsigned long long value;

	if (!get_devtree_value(DEVTREE_CRASHKERNEL_BASE, &value))
		*start = be64_to_cpu(value);
	else
		return -1;

	if (!get_devtree_value(DEVTREE_CRASHKERNEL_SIZE, &value))
		*end = *start + be64_to_cpu(value) - 1;
	else
		return -1;

	return 0;
}

int is_crashkernel_mem_reserved(void)
{
	int fd;

	fd = open(DEVTREE_CRASHKERNEL_BASE, O_RDONLY);
	if (fd < 0)
		return 0;
	close(fd);
	return 1;
}

#if 0
static int sort_regions(mem_rgns_t *rgn)
{
	int i, j;
	unsigned long long tstart, tend;
	for (i = 0; i < rgn->size; i++) {
		for (j = 0; j < rgn->size - i - 1; j++) {
			if (rgn->ranges[j].start > rgn->ranges[j+1].start) {
				tstart = rgn->ranges[j].start;
				tend = rgn->ranges[j].end;
				rgn->ranges[j].start = rgn->ranges[j+1].start;
				rgn->ranges[j].end = rgn->ranges[j+1].end;
				rgn->ranges[j+1].start = tstart;
				rgn->ranges[j+1].end = tend;
			}
		}
	}
	return 0;

}
#endif

