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
#include "kexec-ppc.h"
#include "crashdump-powerpc.h"

#define DEVTREE_CRASHKERNEL_BASE "/proc/device-tree/chosen/linux,crashkernel-base"
#define DEVTREE_CRASHKERNEL_SIZE "/proc/device-tree/chosen/linux,crashkernel-size"

#ifdef CONFIG_PPC64
static struct crash_elf_info elf_info64 = {
class: ELFCLASS64,
data: ELFDATA2MSB,
machine: EM_PPC64,
page_offset: PAGE_OFFSET,
lowmem_limit: MAXMEM,
};
#endif
static struct crash_elf_info elf_info32 = {
class: ELFCLASS32,
data: ELFDATA2MSB,
#ifdef CONFIG_PPC64
machine: EM_PPC64,
#else
machine: EM_PPC,
#endif
page_offset: PAGE_OFFSET,
lowmem_limit: MAXMEM,
};

/* Stores a sorted list of RAM memory ranges for which to create elf headers.
 * A separate program header is created for backup region
 */
static struct memory_range *crash_memory_range;
static int crash_nr_memory_ranges;

/* Define a variable to replace the CRASH_MAX_MEMORY_RANGES macro */
static int crash_max_memory_ranges;

/*
 * Used to save various memory ranges/regions needed for the captured
 * kernel to boot. (lime memmap= option in other archs)
 */
mem_rgns_t usablemem_rgns = {0, NULL};

/* Append a segment to crash_memory_range, splitting it into two if
 * it contains both lowmem and highmem */
static void add_crash_memory_range(unsigned long long start,
		unsigned long long end)
{
#ifndef CONFIG_PPC64
	if (start < elf_info32.lowmem_limit && end > elf_info32.lowmem_limit) {
		add_crash_memory_range(start, elf_info32.lowmem_limit);
		add_crash_memory_range(elf_info32.lowmem_limit, end);
		return;
	}
#endif

	if (crash_nr_memory_ranges < crash_max_memory_ranges) {
		crash_memory_range[crash_nr_memory_ranges].start = start;
		crash_memory_range[crash_nr_memory_ranges].end = end;
		crash_memory_range[crash_nr_memory_ranges].type = RANGE_RAM;
	}

	crash_nr_memory_ranges++;
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
	char fname[256];
	DIR *dir, *dmem;
	int fd;
	struct dirent *dentry, *mentry;
	int n, crash_rng_len = 0;
	unsigned long long start, end, cstart, cend;

	crash_max_memory_ranges = max_memory_ranges + 6;
	crash_rng_len = sizeof(struct memory_range) * crash_max_memory_ranges;

	crash_memory_range = (struct memory_range *) malloc(crash_rng_len);
	if (!crash_memory_range) {
		fprintf(stderr, "Allocation for crash memory range failed\n");
		return -1;
	}
	memset(crash_memory_range, 0, crash_rng_len);
	crash_nr_memory_ranges = 0;

#ifndef CONFIG_BOOKE
	/* create a separate program header for the backup region */
	add_crash_memory_range(BACKUP_SRC_START, BACKUP_SRC_END + 1);
#endif

	dir = opendir(device_tree);
	if (!dir) {
		perror(device_tree);
		goto err;
	}
	while ((dentry = readdir(dir)) != NULL) {
		if (strncmp(dentry->d_name, "memory@", 7)
		    && strcmp(dentry->d_name, "memory"))
			continue;
		strcpy(fname, device_tree);
		strcat(fname, dentry->d_name);
		dmem = opendir(fname);
		if (!dmem) {
			perror(fname);
			closedir(dir);
			goto err;
		}
		while ((mentry = readdir(dmem)) != NULL) {
			if (strcmp(mentry->d_name, "reg"))
				continue;
			strcat(fname, "/reg");
			fd = open(fname, O_RDONLY);
			if (fd < 0) {
				perror(fname);
				closedir(dmem);
				closedir(dir);
				goto err;
			}
			n = read_memory_region_limits(fd, &start, &end);
			/* We are done with fd, close it. */
			close(fd);
			if (n != 0) {
				closedir(dmem);
				closedir(dir);
				goto err;
			}
#ifndef CONFIG_BOOKE
			if (start == 0 && end >= (BACKUP_SRC_END + 1))
				start = BACKUP_SRC_END + 1;
#endif

			/*
			 * Exclude the region that lies within crashkernel.
			 * If memory limit is set then exclude memory region
			 * above it.
			 */

			if (memory_limit) {
				if (start >= memory_limit)
					continue;
				if (end > memory_limit)
					end = memory_limit;
			}

			/*
			 * Exclure region used by crash kernel
			 */
			cstart = crash_base;
			cend = crash_base + crash_size;

			if (cstart >= end || cend <= start)
				add_crash_memory_range(start, end);
			else {
				if (start < cstart)
					add_crash_memory_range(start, cstart);
				if (cend < end)
					add_crash_memory_range(cend, end);
			}
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
		cstart = rtas_base;
		cend = rtas_base + rtas_size;
		if (cstart < crash_base)
			cstart = crash_base;
		if (cend > crash_base + crash_size)
			cend = crash_base + crash_size;
		add_crash_memory_range(cstart, cend);
	}

	if (crash_nr_memory_ranges >= crash_max_memory_ranges) {
		fprintf(stderr,
			"Error: Number of crash memory ranges"
			" excedeed the max limit\n");
		goto err;
	}

	*range = crash_memory_range;
	*ranges = crash_nr_memory_ranges;

	int j;
	dbgprintf("CRASH MEMORY RANGES\n");
	for (j = 0; j < *ranges; j++) {
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

/* Converts unsigned long to ascii string. */
static void ulltoa(unsigned long long i, char *str)
{
	int j = 0, k;
	char tmp;

	do {
		str[j++] = i % 10 + '0';
	} while ((i /= 10) > 0);
	str[j] = '\0';

	/* Reverse the string. */
	for (j = 0, k = strlen(str) - 1; j < k; j++, k--) {
		tmp = str[k];
		str[k] = str[j];
		str[j] = tmp;
	}
}

/* Append str to cmdline */
static void add_cmdline(char *cmdline, char *str)
{
	int cmdline_size;
	int cmdlen = strlen(cmdline) + strlen(str);

	cmdline_size = COMMAND_LINE_SIZE;
	if (cmdlen > (cmdline_size - 1))
		die("Command line overflow\n");
	strcat(cmdline, str);
}

static int add_cmdline_param(char *cmdline, unsigned long long addr,
				char *cmdstr, char *byte)
{
	int align = 1024;
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
	ulltoa(addr, ptr);
	strcat(str, byte);

	add_cmdline(cmdline, str);

	dbgprintf("Command line after adding elfcorehdr: %s\n", cmdline);

	return 0;
}

/* Loads additional segments in case of a panic kernel is being loaded.
 * One segment for backup region, another segment for storing elf headers
 * for crash memory image.
 */
int load_crashdump_segments(struct kexec_info *info, char *mod_cmdline,
				unsigned long max_addr, unsigned long min_base)
{
	void *tmp;
	unsigned long sz, elfcorehdr;
	int nr_ranges, align = 1024, i;
	unsigned long long end;
	struct memory_range *mem_range;

	if (get_crash_memory_ranges(&mem_range, &nr_ranges) < 0)
		return -1;

	info->backup_src_start = BACKUP_SRC_START;
	info->backup_src_size = BACKUP_SRC_SIZE;
#ifndef CONFIG_BOOKE
	/* Create a backup region segment to store backup data*/
	sz = _ALIGN(BACKUP_SRC_SIZE, align);
	tmp = xmalloc(sz);
	memset(tmp, 0, sz);
	info->backup_start = add_buffer(info, tmp, sz, sz, align,
					0, max_addr, 1);
	reserve(info->backup_start, sz);
#endif

	/* On powerpc memory ranges in device-tree is denoted as start
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


#ifdef CONFIG_PPC64
	/* Create elf header segment and store crash image data. */
	if (arch_options.core_header_type == CORE_TYPE_ELF64) {
		if (crash_create_elf64_headers(info, &elf_info64,
					crash_memory_range, nr_ranges, &tmp,
					&sz, ELF_CORE_HEADER_ALIGN) < 0)
			return -1;
	} else if (crash_create_elf32_headers(info, &elf_info32,
				crash_memory_range, nr_ranges, &tmp, &sz,
				ELF_CORE_HEADER_ALIGN) < 0)
			return -1;
#else
	if (crash_create_elf32_headers(info, &elf_info32, crash_memory_range,
				nr_ranges, &tmp, &sz, ELF_CORE_HEADER_ALIGN)
			< 0)
		return -1;
#endif

	elfcorehdr = add_buffer(info, tmp, sz, sz, align,
			min_base, max_addr, 1);
	reserve(elfcorehdr, sz);
	/* modify and store the cmdline in a global array. This is later
	 * read by flatten_device_tree and modified if required
	 */
	add_cmdline_param(mod_cmdline, elfcorehdr, " elfcorehdr=", "K");
	add_cmdline(mod_cmdline, " maxcpus=1");
	return 0;
}

/*
 * Used to save various memory regions needed for the captured kernel.
 */

void add_usable_mem_rgns(unsigned long long base, unsigned long long size)
{
	int i;
	unsigned long long end = base + size;
	unsigned long long ustart, uend;

	base = _ALIGN_DOWN(base, getpagesize());
	end = _ALIGN_UP(end, getpagesize());

	for (i = 0; i < usablemem_rgns.size; i++) {
		ustart = usablemem_rgns.ranges[i].start;
		uend = usablemem_rgns.ranges[i].end;
		if (base < uend && end > ustart) {
			if ((base >= ustart) && (end <= uend))
				return;
			if (base < ustart && end > uend) {
				usablemem_rgns.ranges[i].start = base;
				usablemem_rgns.ranges[i].end = end;
				return;
			} else if (base < ustart) {
				usablemem_rgns.ranges[i].start = base;
				return;
			} else if (end > uend) {
				usablemem_rgns.ranges[i].end = end;
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
		*start = value;
	else
		return -1;

	if (!get_devtree_value(DEVTREE_CRASHKERNEL_SIZE, &value))
		*end = *start + value - 1;
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

