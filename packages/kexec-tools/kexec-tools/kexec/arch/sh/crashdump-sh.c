/*
 * crashdump-sh.c - crashdump for SuperH
 * Copyright (C) 2008 Magnus Damm
 *
 * Based on x86 and ppc64 implementation, written by
 * Vivek Goyal (vgoyal@in.ibm.com), R Sharada (sharada@in.ibm.com)
 * Copyright (C) IBM Corporation, 2005. All rights reserved
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#define _GNU_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <elf.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"
#include "../../kexec-elf-boot.h"
#include "../../kexec-syscall.h"
#include "../../crashdump.h"
#include "kexec-sh.h"
#include "crashdump-sh.h"
#include <arch/options.h>

#define CRASH_MAX_MEMORY_RANGES 64
static struct memory_range crash_memory_range[CRASH_MAX_MEMORY_RANGES];

static int crash_sh_range_nr;
static int crash_sh_memory_range_callback(void *UNUSED(data), int UNUSED(nr),
					  char *str,
					  unsigned long long base,
					  unsigned long long length)
{

	struct memory_range *range = crash_memory_range;
	struct memory_range *range2 = crash_memory_range;

	range += crash_sh_range_nr;

	if (crash_sh_range_nr >= CRASH_MAX_MEMORY_RANGES) {
		return 1;
	}

	if (strncmp(str, "System RAM\n", 11) == 0) {
		range->start = base;
		range->end = base + length - 1;
		range->type = RANGE_RAM;
		crash_sh_range_nr++;
	}

	if (strncmp(str, "Crash kernel\n", 13) == 0) {
		if (!crash_sh_range_nr)
			die("Unsupported /proc/iomem format\n");

		range2 = range - 1;
		if ((base + length - 1) < range2->end) {
			range->start = base + length;
			range->end = range2->end;
			range->type = RANGE_RAM;
			crash_sh_range_nr++;
		}
		range2->end = base - 1;
	}

	return 0;
}

/* Return a sorted list of available memory ranges. */
static int crash_get_memory_ranges(struct memory_range **range, int *ranges)
{
	crash_sh_range_nr = 0;

	kexec_iomem_for_each_line(NULL, crash_sh_memory_range_callback, NULL);
	*range = crash_memory_range;
	*ranges = crash_sh_range_nr;
	return 0;
}

static struct crash_elf_info elf_info32 =
{
	class: ELFCLASS32,
	data: ELFDATA2LSB,
	machine: EM_SH,
	page_offset: PAGE_OFFSET,
};

static int add_cmdline_param(char *cmdline, uint64_t addr, char *cmdstr,
				char *byte)
{
	int cmdlen, len, align = 1024;
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
	if (cmdlen > (COMMAND_LINE_SIZE - 1))
		die("Command line overflow\n");
	strcat(cmdline, str);

	dbgprintf("Command line after adding elfcorehdr: %s\n", cmdline);

	return 0;
}

/* Loads additional segments in case of a panic kernel is being loaded.
 * One segment for storing elf headers for crash memory image.
 */
int load_crashdump_segments(struct kexec_info *info, char* mod_cmdline)
{
	void *tmp;
	unsigned long sz, elfcorehdr;
	int nr_ranges;
	struct memory_range *mem_range;

	if (crash_get_memory_ranges(&mem_range, &nr_ranges) < 0)
		return -1;

	if (crash_create_elf32_headers(info, &elf_info32,
				       mem_range, nr_ranges,
				       &tmp, &sz,
				       ELF_CORE_HEADER_ALIGN) < 0)
		return -1;

	elfcorehdr = add_buffer_phys_virt(info, tmp, sz, sz, 1024,
					  0, 0xffffffff, -1, 0);

	dbgprintf("Created elf header segment at 0x%lx\n", elfcorehdr);
	add_cmdline_param(mod_cmdline, elfcorehdr, " elfcorehdr=", "K");
	add_cmdline_param(mod_cmdline, elfcorehdr - mem_min, " mem=", "K");

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
