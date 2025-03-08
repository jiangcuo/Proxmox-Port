/*
 * kexec/arch/s390/kexec-s390.c
 *
 * Copyright IBM Corp. 2005,2011
 *
 * Author(s): Rolf Adelsberger <adelsberger@de.ibm.com>
 *            Michael Holzheu <holzheu@linux.vnet.ibm.com>
 *
 */

#define _GNU_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <dirent.h>
#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "kexec-s390.h"
#include <arch/options.h>

static struct memory_range memory_range[MAX_MEMORY_RANGES];

/*
 * Read string from file
 */
static void read_str(char *string, const char *path, size_t len)
{
	size_t rc;
	FILE *fh;

	fh = fopen(path, "rb");
	if (fh == NULL)
		die("Could not open \"%s\"", path);
	rc = fread(string, 1, len - 1, fh);
	if (rc == 0 && ferror(fh))
		die("Could not read \"%s\"", path);
	fclose(fh);
	string[rc] = 0;
	if (string[strlen(string) - 1] == '\n')
		string[strlen(string) - 1] = 0;
}

/*
 * Return number of memory chunks
 */
static int memory_range_cnt(struct memory_range chunks[])
{
	int i;

	for (i = 0; i < MAX_MEMORY_RANGES; i++) {
		if (chunks[i].end == 0)
			break;
	}
	return i;
}

/*
 * Create memory hole with given address and size
 *
 * lh = local hole
 */
static void add_mem_hole(struct memory_range chunks[], unsigned long addr,
			 unsigned long size)
{
	unsigned long lh_start, lh_end, lh_size, chunk_cnt;
	int i;

	chunk_cnt = memory_range_cnt(chunks);

	for (i = 0; i < chunk_cnt; i++) {
		if (addr + size <= chunks[i].start)
			break;
		if (addr > chunks[i].end)
			continue;
		lh_start = MAX(addr, chunks[i].start);
		lh_end = MIN(addr + size - 1, chunks[i].end);
		lh_size = lh_end - lh_start + 1;
		if (lh_start == chunks[i].start && lh_end == chunks[i].end) {
			/* Remove chunk */
			memmove(&chunks[i], &chunks[i + 1],
				sizeof(struct memory_range) *
				(MAX_MEMORY_RANGES - (i + 1)));
			memset(&chunks[MAX_MEMORY_RANGES - 1], 0,
			       sizeof(struct memory_range));
			chunk_cnt--;
			i--;
		} else if (lh_start == chunks[i].start) {
			/* Make chunk smaller at start */
			chunks[i].start = chunks[i].start + lh_size;
			break;
		} else if (lh_end == chunks[i].end) {
			/* Make chunk smaller at end */
			chunks[i].end = lh_start - 1;
		} else {
			/* Split chunk into two */
			if (chunk_cnt >= MAX_MEMORY_RANGES)
				die("Unable to create memory hole: %i", i);
			memmove(&chunks[i + 1], &chunks[i],
				sizeof(struct memory_range) *
				(MAX_MEMORY_RANGES - (i + 1)));
			chunks[i + 1].start = lh_start + lh_size;
			chunks[i].end = lh_start - 1;
			break;
		}
	}
}

/*
 * Remove offline memory from memory chunks
 */
static void remove_offline_memory(struct memory_range memory_range[])
{
	unsigned long block_size, chunk_nr;
	struct dirent *dirent;
	char path[PATH_MAX];
	char str[64];
	DIR *dir;

	read_str(str, "/sys/devices/system/memory/block_size_bytes",
		 sizeof(str));
	sscanf(str, "%lx", &block_size);

	dir = opendir("/sys/devices/system/memory");
	if (!dir)
		die("Could not read \"/sys/devices/system/memory\"");
	while ((dirent = readdir(dir))) {
		if (sscanf(dirent->d_name, "memory%ld\n", &chunk_nr) != 1)
			continue;
		sprintf(path, "/sys/devices/system/memory/%s/state",
			dirent->d_name);
		read_str(str, path, sizeof(str));
		if (strncmp(str, "offline", 6) != 0)
			continue;
		add_mem_hole(memory_range, chunk_nr * block_size, block_size);
	}
	closedir(dir);
}

/*
 * Get memory ranges of type "System RAM" from /proc/iomem. If with_crashk=1
 * then also type "Crash kernel" is added.
 */
int get_memory_ranges_s390(struct memory_range memory_range[], int *ranges,
			   int with_crashk)
{
	char crash_kernel[] = "Crash kernel\n";
	char sys_ram[] = "System RAM\n";
	const char *iomem = proc_iomem();
	FILE *fp;
	char line[80];
	int current_range = 0;

	fp = fopen(iomem,"r");
	if(fp == 0) {
		fprintf(stderr,"Unable to open %s: %s\n",iomem,strerror(errno));
		return -1;
	}

	/* Setup the compare string properly. */
	while (fgets(line, sizeof(line), fp) != 0) {
		unsigned long long start, end;
		int cons;
		char *str;

		if (current_range == MAX_MEMORY_RANGES)
			break;

		sscanf(line,"%llx-%llx : %n", &start, &end, &cons);
		str = line+cons;
		if ((memcmp(str, sys_ram, strlen(sys_ram)) == 0) ||
		    ((memcmp(str, crash_kernel, strlen(crash_kernel)) == 0) &&
		     with_crashk)) {
			memory_range[current_range].start = start;
			memory_range[current_range].end = end;
			memory_range[current_range].type = RANGE_RAM;
			current_range++;
		}
		else {
			continue;
		}
	}
	fclose(fp);
	remove_offline_memory(memory_range);
	*ranges = memory_range_cnt(memory_range);
	return 0;
}

/*
 * get_memory_ranges:
 *  Return a list of memory ranges by parsing the file returned by
 *  proc_iomem()
 *
 * INPUT:
 *  - Pointer to an array of memory_range structures.
 *  - Pointer to an integer with holds the number of memory ranges.
 *
 * RETURN:
 *  - 0 on normal execution.
 *  - (-1) if something went wrong.
 */

int get_memory_ranges(struct memory_range **range, int *ranges,
		      unsigned long flags)
{
	uint64_t start, end;

	if (get_memory_ranges_s390(memory_range, ranges,
				   flags & KEXEC_ON_CRASH))
		return -1;
	*range = memory_range;
	if ((flags & KEXEC_ON_CRASH) && !(flags & KEXEC_PRESERVE_CONTEXT)) {
		if (parse_iomem_single("Crash kernel\n", &start, &end))
			return -1;
		if (start > mem_min)
			mem_min = start;
		if (end < mem_max)
			mem_max = end;
	}
	return 0;
}

/* Supported file types and callbacks */
struct file_type file_type[] = {
	{ "image", image_s390_probe, image_s390_load, image_s390_usage},
};
int file_types = sizeof(file_type) / sizeof(file_type[0]);


void arch_usage(void)
{
}

int arch_process_options(int UNUSED(argc), char **UNUSED(argv))
{
	return 0;
}

const struct arch_map_entry arches[] = {
	{ "s390", KEXEC_ARCH_S390 },
	{ "s390x", KEXEC_ARCH_S390 },
	{ NULL, 0 },
};

int arch_compat_trampoline(struct kexec_info *UNUSED(info))
{
	return 0;
}

void arch_update_purgatory(struct kexec_info *UNUSED(info))
{
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

int arch_do_exclude_segment(struct kexec_info *UNUSED(info), struct kexec_segment *UNUSED(segment))
{
	return 0;
}
