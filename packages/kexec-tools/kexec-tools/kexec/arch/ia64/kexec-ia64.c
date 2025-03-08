/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2003-2005  Eric Biederman (ebiederm@xmission.com)
 * Copyright (C) 2004 Albert Herranz
 * Copyright (C) 2004 Silicon Graphics, Inc.
 *   Jesse Barnes <jbarnes@sgi.com>
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

#define _GNU_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <sched.h>
#include <limits.h>
#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "elf.h"
#include "kexec-ia64.h"
#include <arch/options.h>

/* The number of entries in memory_range array is always smaller than
 * the number of entries in the file returned by proc_iomem(),
 * stored in max_memory_ranges. */
static struct memory_range *memory_range;
int max_memory_ranges;
static int memory_ranges;
unsigned long saved_efi_memmap_size;

/* Reserve range for EFI memmap and Boot parameter */
static int split_range(int range, unsigned long start, unsigned long end)
{
	unsigned long ram_end = memory_range[range - 1].end;
	unsigned int type = memory_range[range - 1].type;
	int i;
	//align end and start to page size of EFI
	start = _ALIGN_DOWN(start, 1UL<<12);
	end = _ALIGN(end, 1UL<<12);
	for (i = 0; i < range; i++)
		if(memory_range[i].start <= start && memory_range[i].end >=end)
			break;
	if (i >= range)
		return range;
	range = i;
	if (memory_range[range].start < start) {
		memory_range[range].end = start;
		range++;
	}
	memory_range[range].start = start;
	memory_range[range].end = end;
	memory_range[range].type = RANGE_RESERVED;
	range++;
	if (end < ram_end) {
		memory_range[range].start = end;
		memory_range[range].end = ram_end;
		memory_range[range].type = type;
		range++;
	}
	return range;
}

/* Return a sorted list of available memory ranges. */
int get_memory_ranges(struct memory_range **range, int *ranges,
				unsigned long kexec_flags)
{
	const char *iomem = proc_iomem();
	char line[MAX_LINE];
	FILE *fp;
	fp = fopen(iomem, "r");
	if (!fp) {
		fprintf(stderr, "Cannot open %s: %s\n",
			iomem, strerror(errno));
		return -1;
	}

	/* allocate memory_range dynamically */
	max_memory_ranges = 0;
	while(fgets(line, sizeof(line), fp) != 0) {
		max_memory_ranges++;
	}
	memory_range = xmalloc(sizeof(struct memory_range) *
			max_memory_ranges);
	rewind(fp);

	while(fgets(line, sizeof(line), fp) != 0) {
		unsigned long start, end;
		char *str;
		unsigned type;
		int consumed;
		int count;
		if (memory_ranges >= max_memory_ranges)
			break;
		count = sscanf(line, "%lx-%lx : %n",
				&start, &end, &consumed);
		if (count != 2)
			continue;
		str = line + consumed;
		end = end + 1;
		if (memcmp(str, "System RAM\n", 11) == 0) {
			type = RANGE_RAM;
		}
		else if (memcmp(str, "reserved\n", 9) == 0) {
			type = RANGE_RESERVED;
		}
		else if (memcmp(str, "Crash kernel\n", 13) == 0) {
			/* Redefine the memory region boundaries if kernel
			 * exports the limits and if it is panic kernel.
			 * Override user values only if kernel exported
			 * values are subset of user defined values.
			 */

			if (kexec_flags & KEXEC_ON_CRASH) {
				if (start > mem_min)
					mem_min = start;
				if (end < mem_max)
					mem_max = end;
			}
			continue;
		} else if (memcmp(str, "Boot parameter\n", 14) == 0) {
			memory_ranges = split_range(memory_ranges, start, end);
			continue;
		} else if (memcmp(str, "EFI Memory Map\n", 14) == 0) {
			memory_ranges = split_range(memory_ranges, start, end);
			saved_efi_memmap_size = end - start;
			continue;
		} else if (memcmp(str, "Uncached RAM\n", 13) == 0) {
			type = RANGE_UNCACHED;
		} else {
			continue;
		}
		/*
		 * Check if this memory range can be coalesced with
		 * the previous range
		 */
		if ((memory_ranges > 0) &&
			(start == memory_range[memory_ranges-1].end) &&
			(type == memory_range[memory_ranges-1].type)) {
			memory_range[memory_ranges-1].end = end;
		}
		else {
			memory_range[memory_ranges].start = start;
			memory_range[memory_ranges].end = end;
			memory_range[memory_ranges].type = type;
			memory_ranges++;
		}
	}
	fclose(fp);
 	*range = memory_range;
 	*ranges = memory_ranges;

 	return 0;
}

/* Supported file types and callbacks */
struct file_type file_type[] = {
       {"elf-ia64", elf_ia64_probe, elf_ia64_load, elf_ia64_usage},
};
int file_types = sizeof(file_type) / sizeof(file_type[0]);


void arch_usage(void)
{
}

int arch_process_options(int argc, char **argv)
{
	/* This doesn't belong here!  Some sort of arch_init() ? */

	/* execute from monarch processor */
	cpu_set_t affinity;
	CPU_ZERO(&affinity);
	CPU_SET(0, &affinity);
	sched_setaffinity(0, sizeof(affinity), &affinity);

	return 0;
}

const struct arch_map_entry arches[] = {
	{ "ia64", KEXEC_ARCH_IA_64 },
	{ NULL, 0 },
};

int arch_compat_trampoline(struct kexec_info *UNUSED(info))
{
	return 0;
}

int update_loaded_segments(struct mem_ehdr *ehdr)
{
	int i;
	unsigned u;
	struct mem_phdr *phdr;
	unsigned long start_addr = ULONG_MAX, end_addr = 0;
	unsigned long align = 1UL<<26; /* 64M */
	unsigned long start, end;

	for (u = 0; u < ehdr->e_phnum; u++) {
		phdr = &ehdr->e_phdr[u];
		if (phdr->p_type != PT_LOAD)
			continue;
		if (phdr->p_paddr < start_addr)
			start_addr = phdr->p_paddr;
		if ((phdr->p_paddr + phdr->p_memsz) > end_addr)
			end_addr = phdr->p_paddr + phdr->p_memsz;
	}

	for (i = 0; i < memory_ranges && memory_range[i].start <= start_addr;
	     i++) {
		if (memory_range[i].type == RANGE_RAM &&
		    memory_range[i].end > end_addr)
			return 0;
	}

	for (i = 0; i < memory_ranges; i++) {
		if (memory_range[i].type != RANGE_RAM)
			continue;
		start = _ALIGN(memory_range[i].start, align);
		end = memory_range[i].end;
		if (end > start && (end - start) > (end_addr - start_addr)) {
		    move_loaded_segments(ehdr, start);
			return 0;
		}
	}

	return -1;
}

void arch_update_purgatory(struct kexec_info *UNUSED(info))
{
}

int arch_do_exclude_segment(struct kexec_info *UNUSED(info), struct kexec_segment *UNUSED(segment))
{
	return 0;
}
