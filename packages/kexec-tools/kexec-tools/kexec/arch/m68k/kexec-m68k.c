/*
 * kexec-m68k.c - kexec for m68k
 *
 * Copyright (C) 2013 Geert Uytterhoeven
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
#include "kexec-m68k.h"
#include "bootinfo.h"
#include <arch/options.h>


static unsigned long m68k_memoffset;


/* Return a sorted list of memory ranges. */
int get_memory_ranges(struct memory_range **range, int *ranges,
		      unsigned long kexec_flags)
{
	bootinfo_load();
	*ranges = bootinfo_get_memory_ranges(range);
	m68k_memoffset = (*range)[0].start;
	return 0;
}


struct file_type file_type[] = {
	{"elf-m68k", elf_m68k_probe, elf_m68k_load, elf_m68k_usage},
};
int file_types = sizeof(file_type) / sizeof(file_type[0]);

void arch_usage(void)
{
}

int arch_process_options(int argc, char **argv)
{
	static const struct option options[] = {
		KEXEC_ALL_OPTIONS
		{ "bootinfo",		1, NULL, OPT_BOOTINFO },
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
		case OPT_BOOTINFO:
			bootinfo_file = optarg;
			break;
		}
	}
	/* Reset getopt for the next pass; called in other source modules */
	opterr = 1;
	optind = 1;
	return 0;
}

const struct arch_map_entry arches[] = {
	{ "m68k", KEXEC_ARCH_68K },
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

unsigned long virt_to_phys(unsigned long addr)
{
	return addr + m68k_memoffset;
}

/*
 * add_segment() should convert base to a physical address on m68k,
 * while the default is just to work with base as is */
void add_segment(struct kexec_info *info, const void *buf, size_t bufsz,
		 unsigned long base, size_t memsz)
{
	add_segment_phys_virt(info, buf, bufsz, base, memsz, 1);
}

int arch_do_exclude_segment(struct kexec_info *UNUSED(info), struct kexec_segment *UNUSED(segment))
{
	return 0;
}
