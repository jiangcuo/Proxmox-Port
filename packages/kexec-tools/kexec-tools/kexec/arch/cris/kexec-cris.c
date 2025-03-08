/*
 * kexec-cris.c 
 * Copyright (C) 2008 AXIS Communications AB
 * Written by Edgar E. Iglesias
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
#include "kexec-cris.h"
#include <arch/options.h>

#define MAX_MEMORY_RANGES  64
#define MAX_LINE          160
static struct memory_range memory_range[MAX_MEMORY_RANGES];

/* Return a sorted list of memory ranges. */
int get_memory_ranges(struct memory_range **range, int *ranges,
		      unsigned long UNUSED(kexec_flags))
{
	int memory_ranges = 0;

	memory_range[memory_ranges].start = 0x40000000;
	memory_range[memory_ranges].end   = 0x41000000;
	memory_range[memory_ranges].type = RANGE_RAM;
	memory_ranges++;

	memory_range[memory_ranges].start = 0xc0000000;
	memory_range[memory_ranges].end   = 0xc1000000;
	memory_range[memory_ranges].type = RANGE_RAM;
	memory_ranges++;

	*range = memory_range;
	*ranges = memory_ranges;
	return 0;
}

struct file_type file_type[] = {
	{"elf-cris", elf_cris_probe, elf_cris_load, elf_cris_usage},
};
int file_types = sizeof(file_type) / sizeof(file_type[0]);

void arch_usage(void)
{
}

int arch_process_options(int argc, char **argv)
{
	return 0;
}

const struct arch_map_entry arches[] = {
	{ "cris", KEXEC_ARCH_CRIS },
	{ "crisv32", KEXEC_ARCH_CRIS },
	{ 0 },
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
	return (addr) & 0x7fffffff;
}

/*
 * add_segment() should convert base to a physical address on cris,
 * while the default is just to work with base as is */
void add_segment(struct kexec_info *info, const void *buf, size_t bufsz,
                 unsigned long base, size_t memsz)
{
        add_segment_phys_virt(info, buf, bufsz, base, memsz, 1);
}

/*
 * add_buffer() should convert base to a physical address on cris,
 * while the default is just to work with base as is */
unsigned long add_buffer(struct kexec_info *info, const void *buf,
                         unsigned long bufsz, unsigned long memsz,
                         unsigned long buf_align, unsigned long buf_min,
                         unsigned long buf_max, int buf_end)
{
        return add_buffer_phys_virt(info, buf, bufsz, memsz, buf_align,
                                    buf_min, buf_max, buf_end, 1);
}

int arch_do_exclude_segment(struct kexec_info *UNUSED(info), struct kexec_segment *UNUSED(segment))
{
	return 0;
}
