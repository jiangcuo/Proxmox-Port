/*
 * kexec-hppa.c - kexec for hppa
 *
 * Copyright (C) 2019 Sven Schnelle <svens@stackframe.org>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "kexec-hppa.h"
#include <arch/options.h>

#define SYSTEM_RAM "System RAM\n"
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static struct memory_range memory_range[MAX_MEMORY_RANGES];
unsigned long phys_offset;

/* Return a sorted list of available memory ranges. */
int get_memory_ranges(struct memory_range **range, int *ranges,
		unsigned long UNUSED(kexec_flags))
{
	const char *iomem = proc_iomem();
	int memory_ranges = 0;
	char line[512];
	FILE *fp;

	fp = fopen(iomem, "r");

	if (!fp) {
		fprintf(stderr, "Cannot open %s: %s\n",
			iomem, strerror(errno));
		return -1;
	}

	while(fgets(line, sizeof(line), fp) != 0) {
		unsigned long long start, end;
		char *str;
		int type;
		int consumed;
		int count;


		count = sscanf(line, "%llx-%llx : %n", &start, &end, &consumed);

		if (count != 2)
			continue;

		str = line + consumed;

		if (memcmp(str, SYSTEM_RAM, strlen(SYSTEM_RAM)) == 0) {
			type = RANGE_RAM;
		} else if (memcmp(str, "reserved\n", 9) == 0) {
			type = RANGE_RESERVED;
		} else {
			continue;
		}

		memory_range[memory_ranges].start = start;
		memory_range[memory_ranges].end = end;
		memory_range[memory_ranges].type = type;
		if (++memory_ranges >= MAX_MEMORY_RANGES)
			break;
	}
	fclose(fp);
	*range = memory_range;
	*ranges = memory_ranges;

	dbgprint_mem_range("MEMORY RANGES", *range, *ranges);
	return 0;
}

struct file_type file_type[] = {
	{"elf-hppa", elf_hppa_probe, elf_hppa_load, elf_hppa_usage},
};
int file_types = ARRAY_SIZE(file_type);

void arch_usage(void)
{
}

int arch_process_options(int argc, char **argv)
{
	static const struct option options[] = {
		KEXEC_ALL_OPTIONS
		{ 0,			0, NULL, 0 },
	};
	static const char short_options[] = KEXEC_ALL_OPT_STR;
	int opt;

	opterr = 0; /* Don't complain about unrecognized options here */
	while ((opt = getopt_long(argc, argv, short_options, options, 0)) !=
		-1) {
		switch (opt) {
		default:
			break;
		}
	}
	/* Reset getopt for the next pass; called in other source modules */
	opterr = 1;
	optind = 1;
	return 0;
}

const struct arch_map_entry arches[] = {
	{ "parisc64", KEXEC_ARCH_HPPA },
	{ "parisc", KEXEC_ARCH_HPPA },
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
	return 0;
}

int get_crash_kernel_load_range(uint64_t *start, uint64_t *end)
{
	/* Crash kernel region size is not exposed by the system */
	return -1;
}

void add_segment(struct kexec_info *info, const void *buf, size_t bufsz,
	unsigned long base, size_t memsz)
{
	add_segment_phys_virt(info, buf, bufsz, base, memsz, 1);
}

unsigned long virt_to_phys(unsigned long addr)
{
	return addr - phys_offset;
}

int arch_do_exclude_segment(struct kexec_info *UNUSED(info), struct kexec_segment *UNUSED(segment))
{
	return 0;
}
