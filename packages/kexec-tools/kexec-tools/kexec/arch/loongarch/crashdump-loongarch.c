/*
 * LoongArch crashdump.
 *
 * Copyright (C) 2022 Loongson Technology Corporation Limited.
 *   Youling Tang <tangyouling@loongson.cn>
 *
 * derived from crashdump-arm64.c
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <linux/elf.h>

#include "kexec.h"
#include "crashdump.h"
#include "crashdump-loongarch.h"
#include "iomem.h"
#include "kexec-loongarch.h"
#include "kexec-elf.h"
#include "mem_regions.h"

/* memory ranges of crashed kernel */
static struct memory_ranges system_memory_rgns;

/* memory range reserved for crashkernel */
struct memory_range crash_reserved_mem[CRASH_MAX_RESERVED_RANGES];
struct memory_ranges usablemem_rgns = {
	.size = 0,
	.max_size = CRASH_MAX_RESERVED_RANGES,
	.ranges = crash_reserved_mem,
};

struct memory_range elfcorehdr_mem;

static struct crash_elf_info elf_info64 = {
	.class		= ELFCLASS64,
	.data		= ELFDATA2LSB,
	.machine	= EM_LOONGARCH,
	.page_offset	= PAGE_OFFSET,
};

/*
 * iomem_range_callback() - callback called for each iomem region
 * @data: not used
 * @nr: not used
 * @str: name of the memory region
 * @base: start address of the memory region
 * @length: size of the memory region
 *
 * This function is called once for each memory region found in /proc/iomem.
 * It locates system RAM and crashkernel reserved memory and places these to
 * variables, respectively, system_memory_rgns and usablemem_rgns.
 */

static int iomem_range_callback(void *UNUSED(data), int UNUSED(nr),
				char *str, unsigned long long base,
				unsigned long long length)
{
	if (strncmp(str, CRASH_KERNEL, strlen(CRASH_KERNEL)) == 0)
		return mem_regions_alloc_and_add(&usablemem_rgns,
						base, length, RANGE_RAM);
	else if (strncmp(str, SYSTEM_RAM, strlen(SYSTEM_RAM)) == 0)
		return mem_regions_alloc_and_add(&system_memory_rgns,
						base, length, RANGE_RAM);
	else if (strncmp(str, KERNEL_CODE, strlen(KERNEL_CODE)) == 0)
		elf_info64.kern_paddr_start = base;
	else if (strncmp(str, KERNEL_DATA, strlen(KERNEL_DATA)) == 0)
		elf_info64.kern_size = base + length - elf_info64.kern_paddr_start;

	return 0;
}

int is_crashkernel_mem_reserved(void)
{
	if (!usablemem_rgns.size)
		kexec_iomem_for_each_line(NULL, iomem_range_callback, NULL);

	return usablemem_rgns.size;
}

/*
 * crash_get_memory_ranges() - read system physical memory
 *
 * Function reads through system physical memory and stores found memory
 * regions in system_memory_ranges.
 * Regions are sorted in ascending order.
 *
 * Returns 0 in case of success and a negative value otherwise.
 */
static int crash_get_memory_ranges(void)
{
	int i;

	/*
	 * First read all memory regions that can be considered as
	 * system memory including the crash area.
	 */
	if (!usablemem_rgns.size)
		kexec_iomem_for_each_line(NULL, iomem_range_callback, NULL);

	/* allow one or two regions for crash dump kernel */
	if (!usablemem_rgns.size)
		return -EINVAL;

	dbgprint_mem_range("Reserved memory range",
			usablemem_rgns.ranges, usablemem_rgns.size);

	for (i = 0; i < usablemem_rgns.size; i++) {
		if (mem_regions_alloc_and_exclude(&system_memory_rgns,
					&crash_reserved_mem[i])) {
			fprintf(stderr, "Cannot allocate memory for ranges\n");
			return -ENOMEM;
		}
	}

	/*
	 * Make sure that the memory regions are sorted.
	 */
	mem_regions_sort(&system_memory_rgns);

	dbgprint_mem_range("Coredump memory ranges",
			   system_memory_rgns.ranges, system_memory_rgns.size);

	/*
	 * For additional kernel code/data segment.
	 * kern_paddr_start/kern_size are determined in iomem_range_callback
	 */
	elf_info64.kern_vaddr_start = get_kernel_sym("_text");
	if (!elf_info64.kern_vaddr_start)
		elf_info64.kern_vaddr_start = UINT64_MAX;

	return 0;
}

/*
 * load_crashdump_segments() - load the elf core header
 * @info: kexec info structure
 *
 * This function creates and loads an additional segment of elf core header
 : which is used to construct /proc/vmcore on crash dump kernel.
 *
 * Return 0 in case of success and -1 in case of error.
 */

int load_crashdump_segments(struct kexec_info *info)
{
	unsigned long elfcorehdr;
	unsigned long bufsz;
	void *buf;
	int err;

	/*
	 * First fetch all the memory (RAM) ranges that we are going to
	 * pass to the crash dump kernel during panic.
	 */

	err = crash_get_memory_ranges();

	if (err)
		return EFAILED;

	err = crash_create_elf64_headers(info, &elf_info64,
			system_memory_rgns.ranges, system_memory_rgns.size,
			&buf, &bufsz, ELF_CORE_HEADER_ALIGN);

	if (err)
		return EFAILED;

	elfcorehdr = add_buffer(info, buf, bufsz, bufsz, 1024,
		crash_reserved_mem[usablemem_rgns.size - 1].start,
		crash_reserved_mem[usablemem_rgns.size - 1].end, -1);

	elfcorehdr_mem.start = elfcorehdr;
	elfcorehdr_mem.end = elfcorehdr + bufsz - 1;

	dbgprintf("%s: elfcorehdr 0x%llx-0x%llx\n", __func__,
			elfcorehdr_mem.start, elfcorehdr_mem.end);

	return 0;
}

/*
 * e_entry and p_paddr are actually in virtual address space.
 * Those values will be translated to physcal addresses by using
 * virt_to_phys() in add_segment().
 * So let's fix up those values for later use so the memory base will be
 * correctly replaced with crash_reserved_mem[usablemem_rgns.size - 1].start.
 */
void fixup_elf_addrs(struct mem_ehdr *ehdr)
{
	struct mem_phdr *phdr;
	int i;

	ehdr->e_entry += crash_reserved_mem[usablemem_rgns.size - 1].start;

	for (i = 0; i < ehdr->e_phnum; i++) {
		phdr = &ehdr->e_phdr[i];
		if (phdr->p_type != PT_LOAD)
			continue;
		phdr->p_paddr += crash_reserved_mem[usablemem_rgns.size - 1].start;
	}
}

int get_crash_kernel_load_range(uint64_t *start, uint64_t *end)
{
	if (!usablemem_rgns.size)
		kexec_iomem_for_each_line(NULL, iomem_range_callback, NULL);

	if (!usablemem_rgns.size)
		return -1;

	*start = crash_reserved_mem[usablemem_rgns.size - 1].start;
	*end = crash_reserved_mem[usablemem_rgns.size - 1].end;

	return 0;
}
