/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) Nokia Corporation, 2010.
 * Author: Mika Westerberg
 *
 * Based on x86 implementation
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
#include <limits.h>
#include <elf.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../kexec.h"
#include "../../kexec-elf.h"
#include "../../crashdump.h"
#include "../../mem_regions.h"
#include "crashdump-arm.h"
#include "iomem.h"
#include "phys_to_virt.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ELFDATANATIVE ELFDATA2LSB
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ELFDATANATIVE ELFDATA2MSB
#else
#error "Unknown machine endian"
#endif

/*
 * Used to save various memory ranges/regions needed for the captured
 * kernel to boot. (lime memmap= option in other archs)
 */
static struct memory_range crash_memory_ranges[CRASH_MAX_MEMORY_RANGES];
struct memory_ranges usablemem_rgns = {
	.max_size = CRASH_MAX_MEMORY_RANGES,
	.ranges = crash_memory_ranges,
};

/* The boot-time physical memory range reserved for crashkernel region */
struct memory_range crash_kernel_mem;

/* reserved regions */
#define CRASH_MAX_RESERVED_RANGES 2
static struct memory_range crash_reserved_ranges[CRASH_MAX_RESERVED_RANGES];
static struct memory_ranges crash_reserved_rgns = {
	.max_size = CRASH_MAX_RESERVED_RANGES,
	.ranges = crash_reserved_ranges,
};

struct memory_range elfcorehdr_mem;

static struct crash_elf_info elf_info = {
	.class		= ELFCLASS32,
	.data		= ELFDATANATIVE,
	.machine	= EM_ARM,
	.page_offset	= DEFAULT_PAGE_OFFSET,
};

extern unsigned long long user_page_offset;

static int get_kernel_page_offset(struct kexec_info *info,
		struct crash_elf_info *elf_info)
{
	unsigned long long stext_sym_addr = get_kernel_sym("_stext");
	if (stext_sym_addr == 0) {
		if (user_page_offset != (-1ULL)) {
			elf_info->page_offset = user_page_offset;
			dbgprintf("Unable to get _stext symbol from /proc/kallsyms, "
					"use user provided vaule: %llx\n",
					elf_info->page_offset);
			return 0;
		}
		elf_info->page_offset = (unsigned long long)DEFAULT_PAGE_OFFSET;
		dbgprintf("Unable to get _stext symbol from /proc/kallsyms, "
				"use default: %llx\n",
				elf_info->page_offset);
		return 0;
	} else if ((user_page_offset != (-1ULL)) &&
			(user_page_offset != stext_sym_addr)) {
		fprintf(stderr, "PAGE_OFFSET is set to %llx "
				"instead of user provided value %llx\n",
				stext_sym_addr & (~KVBASE_MASK),
				user_page_offset);
	}
	elf_info->page_offset = stext_sym_addr & (~KVBASE_MASK);
	return 0;
}

/**
 * crash_get_memory_ranges() - read system physical memory
 *
 * Function reads through system physical memory and stores found memory regions
 * in @crash_memory_ranges. Number of memory regions found is placed in
 * @crash_memory_nr_ranges. Regions are sorted in ascending order.
 *
 * Returns %0 in case of success and %-1 otherwise (errno is set).
 */
static int crash_get_memory_ranges(void)
{
	int i;

	if (usablemem_rgns.size < 1) {
		errno = EINVAL;
		return -1;
	}

	dbgprint_mem_range("Reserved memory ranges",
			   crash_reserved_rgns.ranges,
			   crash_reserved_rgns.size);

	/*
	 * Exclude all reserved memory from the usable memory regions.
	 * We want to avoid dumping the crashkernel region itself.  Note
	 * that this may result memory regions in usablemem_rgns being
	 * split.
	 */
	for (i = 0; i < crash_reserved_rgns.size; i++) {
		if (mem_regions_exclude(&usablemem_rgns,
					&crash_reserved_rgns.ranges[i])) {
			fprintf(stderr,
				"Error: Number of crash memory ranges excedeed the max limit\n");
			errno = ENOMEM;
			return -1;
		}
	}

	/*
	 * Make sure that the memory regions are sorted.
	 */
	mem_regions_sort(&usablemem_rgns);

	dbgprint_mem_range("Coredump memory ranges",
			   usablemem_rgns.ranges, usablemem_rgns.size);

	return 0;
}

/**
 * cmdline_add_elfcorehdr() - adds elfcorehdr= to @cmdline
 * @cmdline: buffer where parameter is placed
 * @elfcorehdr: physical address of elfcorehdr
 *
 * Function appends 'elfcorehdr=start' at the end of the command line given in
 * @cmdline. Note that @cmdline must be at least %COMMAND_LINE_SIZE bytes long
 * (inclunding %NUL).
 */
static void cmdline_add_elfcorehdr(char *cmdline, unsigned long elfcorehdr)
{
	char buf[COMMAND_LINE_SIZE];
	int buflen;

	buflen = snprintf(buf, sizeof(buf), "%s elfcorehdr=%#lx",
			  cmdline, elfcorehdr);
	if (buflen < 0)
		die("Failed to construct elfcorehdr= command line parameter\n");
	if (buflen >= sizeof(buf))
		die("Command line overflow\n");

	(void) strncpy(cmdline, buf, COMMAND_LINE_SIZE);
	cmdline[COMMAND_LINE_SIZE - 1] = '\0';
}

/**
 * cmdline_add_mem() - adds mem= parameter to kernel command line
 * @cmdline: buffer where parameter is placed
 * @size: size of the kernel reserved memory (in bytes)
 *
 * This function appends 'mem=size' at the end of the command line given in
 * @cmdline. Note that @cmdline must be at least %COMMAND_LINE_SIZE bytes long
 * (including %NUL).
 */
static void cmdline_add_mem(char *cmdline, unsigned long size)
{
	char buf[COMMAND_LINE_SIZE];
	int buflen;

	buflen = snprintf(buf, sizeof(buf), "%s mem=%ldK", cmdline, size >> 10);
	if (buflen < 0)
		die("Failed to construct mem= command line parameter\n");
	if (buflen >= sizeof(buf))
		die("Command line overflow\n");

	(void) strncpy(cmdline, buf, COMMAND_LINE_SIZE);
	cmdline[COMMAND_LINE_SIZE - 1] = '\0';
}

static unsigned long long range_size(const struct memory_range *r)
{
	return r->end - r->start + 1;
}

static void dump_memory_ranges(void)
{
	int i;

	if (!kexec_debug)
		return;

	dbgprintf("crashkernel: [%#llx - %#llx] (%ldM)\n",
		  crash_kernel_mem.start, crash_kernel_mem.end,
		  (unsigned long)range_size(&crash_kernel_mem) >> 20);

	for (i = 0; i < usablemem_rgns.size; i++) {
		struct memory_range *r = usablemem_rgns.ranges + i;
		dbgprintf("memory range: [%#llx - %#llx] (%ldM)\n",
			  r->start, r->end, (unsigned long)range_size(r) >> 20);
	}
}

/**
 * load_crashdump_segments() - loads additional segments needed for kdump
 * @info: kexec info structure
 * @mod_cmdline: kernel command line
 *
 * This function loads additional segments which are needed for the dump capture
 * kernel. It also updates kernel command line passed in @mod_cmdline to have
 * right parameters for the dump capture kernel.
 *
 * Return %0 in case of success and %-1 in case of error.
 */
int load_crashdump_segments(struct kexec_info *info, char *mod_cmdline)
{
	unsigned long elfcorehdr;
	unsigned long bufsz;
	void *buf;
	int err;
	int last_ranges;
	unsigned short align_bit_shift = 20;

	/*
	 * First fetch all the memory (RAM) ranges that we are going to pass to
	 * the crashdump kernel during panic.
	 */
	err = crash_get_memory_ranges();
	if (err)
		return err;

	/*
	 * Now that we have memory regions sorted, we can use first memory
	 * region as PHYS_OFFSET.
	 */
	phys_offset = usablemem_rgns.ranges->start;

	if (get_kernel_page_offset(info, &elf_info))
		return -1;

	dbgprintf("phys offset = %#llx, page offset = %llx\n",
		  phys_offset, elf_info.page_offset);

	/*
	 * Ensure that the crash kernel memory range is sane. The crash kernel
	 * must be located within memory which is visible during booting.
	 */
	if (crash_kernel_mem.end > ARM_MAX_VIRTUAL) {
		fprintf(stderr,
			"Crash kernel memory [0x%llx-0x%llx] is inaccessible at boot - unable to load crash kernel\n",
			crash_kernel_mem.start, crash_kernel_mem.end);
		return -1;
	}

	last_ranges = usablemem_rgns.size - 1;
	if (last_ranges < 0)
		last_ranges = 0;

	if (crash_memory_ranges[last_ranges].end > UINT32_MAX) {
		dbgprintf("Using 64-bit ELF core format\n");

		/* for support LPAE enabled kernel*/
		elf_info.class = ELFCLASS64;
		align_bit_shift = 21;

		err = crash_create_elf64_headers(info, &elf_info,
					 usablemem_rgns.ranges,
					 usablemem_rgns.size, &buf, &bufsz,
					 ELF_CORE_HEADER_ALIGN);
	} else {
		dbgprintf("Using 32-bit ELF core format\n");
		err = crash_create_elf32_headers(info, &elf_info,
					 usablemem_rgns.ranges,
					 usablemem_rgns.size, &buf, &bufsz,
					 ELF_CORE_HEADER_ALIGN);
	}
	if (err)
		return err;

	/*
	 * We allocate ELF core header from the end of the memory area reserved
	 * for the crashkernel. We align the header to SECTION_SIZE (which is
	 * 1MB) so that available memory passed in kernel command line will be
	 * aligned to 1MB. This is because kernel create_mapping() wants memory
	 * regions to be aligned to SECTION_SIZE.
	 * The SECTION_SIZE of LPAE kernel is '1UL << 21' defined in pgtable-3level.h
	 */
	elfcorehdr = add_buffer_phys_virt(info, buf, bufsz, bufsz, 1 << align_bit_shift,
					  crash_kernel_mem.start,
					  crash_kernel_mem.end, -1, 0);

	elfcorehdr_mem.start = elfcorehdr;
	elfcorehdr_mem.end = elfcorehdr + bufsz - 1;

	dbgprintf("elfcorehdr 0x%llx-0x%llx\n", elfcorehdr_mem.start,
		  elfcorehdr_mem.end);
	cmdline_add_elfcorehdr(mod_cmdline, elfcorehdr);

	/*
	 * Add 'mem=size' parameter to dump capture kernel command line. This
	 * prevents the dump capture kernel from using any other memory regions
	 * which belong to the primary kernel.
	 */
	cmdline_add_mem(mod_cmdline, elfcorehdr - crash_kernel_mem.start);

	dump_memory_ranges();
	dbgprintf("kernel command line: \"%s\"\n", mod_cmdline);

	return 0;
}

/**
 * iomem_range_callback() - callback called for each iomem region
 * @data: not used
 * @nr: not used
 * @str: name of the memory region (not NULL terminated)
 * @base: start address of the memory region
 * @length: size of the memory region
 *
 * This function is called for each memory range in /proc/iomem, stores
 * the location of the crash kernel range into @crash_kernel_mem, and
 * stores the system RAM into @usablemem_rgns.
 */
static int iomem_range_callback(void *UNUSED(data), int UNUSED(nr),
				char *str, unsigned long long base,
				unsigned long long length)
{
	if (strncmp(str, CRASH_KERNEL_BOOT, strlen(CRASH_KERNEL_BOOT)) == 0) {
		crash_kernel_mem.start = base;
		crash_kernel_mem.end = base + length - 1;
		crash_kernel_mem.type = RANGE_RAM;
		return mem_regions_add(&crash_reserved_rgns,
				       base, length, RANGE_RAM);
	}
	else if (strncmp(str, CRASH_KERNEL, strlen(CRASH_KERNEL)) == 0) {
		if (crash_kernel_mem.start == crash_kernel_mem.end) {
			crash_kernel_mem.start = base;
			crash_kernel_mem.end = base + length - 1;
			crash_kernel_mem.type = RANGE_RAM;
		}
		return mem_regions_add(&crash_reserved_rgns,
				       base, length, RANGE_RAM);
	}
	else if (strncmp(str, SYSTEM_RAM, strlen(SYSTEM_RAM)) == 0) {
		return mem_regions_add(&usablemem_rgns,
				       base, length, RANGE_RAM);
	}
	return 0;
}

/**
 * is_crashkernel_mem_reserved() - check for the crashkernel reserved region
 *
 * Check for the crashkernel reserved region in /proc/iomem, and return
 * true if it is present, or false otherwise. We use this to store the
 * location of this region, and system RAM regions.
 */
int is_crashkernel_mem_reserved(void)
{
	kexec_iomem_for_each_line(NULL, iomem_range_callback, NULL);

	return crash_kernel_mem.start != crash_kernel_mem.end;
}

int get_crash_kernel_load_range(uint64_t *start, uint64_t *end)
{
	return parse_iomem_single("Crash kernel\n", start, end);
}
