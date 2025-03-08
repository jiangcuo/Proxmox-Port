/*
 * kexec: crashdump support
 * Copyright (C) 2005-2006 Zou Nan hai <nanhai.zou@intel.com> Intel Corp
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
#include <errno.h>
#include <limits.h>
#include <elf.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"
#include "../../kexec-syscall.h"
#include "kexec-ia64.h"
#include "crashdump-ia64.h"
#include "../kexec/crashdump.h"

int memory_ranges = 0;
#define LOAD_OFFSET 	(0xa000000000000000UL + 0x100000000UL -		\
			 kernel_code_start)

static struct crash_elf_info elf_info =
{
	class: ELFCLASS64,
	data: ELFDATA2LSB,
	machine: EM_IA_64,
	page_offset: PAGE_OFFSET,
};

/* Stores a sorted list of RAM memory ranges for which to create elf headers.
 * A separate program header is created for backup region.
 * The number of entries in memory_range array is always smaller than
 * the number of entries in the file returned by proc_iomem(),
 * stored in max_memory_ranges. */
static struct memory_range *crash_memory_range;
/* Memory region reserved for storing panic kernel and other data. */
static struct memory_range crash_reserved_mem;
unsigned long elfcorehdr;
static unsigned long kernel_code_start;
static unsigned long kernel_code_end;
struct loaded_segment {
        unsigned long start;
        unsigned long end;
};

#define MAX_LOAD_SEGMENTS	128
struct loaded_segment loaded_segments[MAX_LOAD_SEGMENTS];

unsigned long loaded_segments_num, loaded_segments_base;
static int seg_comp(const void *a, const void *b)
{
        const struct loaded_segment *x = a, *y = b;
        /* avoid overflow */
        if (x->start > y->start) return 1;
	if (x->start < y->start) return -1;
	return 0;
}

/* purgatory code need this info to patch the EFI memmap
 */
static void add_loaded_segments_info(struct mem_ehdr *ehdr)
{
	unsigned i = 0;
	while(i < ehdr->e_phnum) {
                struct mem_phdr *phdr;
                phdr = &ehdr->e_phdr[i];
		if (phdr->p_type != PT_LOAD) {
			i++;
                        continue;
		}

		loaded_segments[loaded_segments_num].start =
			_ALIGN_DOWN(phdr->p_paddr, ELF_PAGE_SIZE);
                loaded_segments[loaded_segments_num].end =
			loaded_segments[loaded_segments_num].start;

		/* Consolidate consecutive PL_LOAD segments into one.
		 * The end addr of the last PL_LOAD segment, calculated by
		 * adding p_memsz to p_paddr & rounded up to ELF_PAGE_SIZE,
		 * will be the end address of this loaded_segments entry.
		 */
		while (i < ehdr->e_phnum) {
			phdr = &ehdr->e_phdr[i];
	                if (phdr->p_type != PT_LOAD)
	                        break;
			loaded_segments[loaded_segments_num].end =
				_ALIGN(phdr->p_paddr + phdr->p_memsz,
				       ELF_PAGE_SIZE);
			i++;
		}
		loaded_segments_num++;
	}
}

/* Removes crash reserve region from list of memory chunks for whom elf program
 * headers have to be created. Assuming crash reserve region to be a single
 * continuous area fully contained inside one of the memory chunks */
static int exclude_crash_reserve_region(int *nr_ranges)
{
	int i, j, tidx = -1;
	unsigned long cstart, cend;
	struct memory_range temp_region;

	/* Crash reserved region. */
	cstart = crash_reserved_mem.start;
	cend = crash_reserved_mem.end;

	for (i = 0; i < (*nr_ranges); i++) {
		unsigned long mstart, mend;
		mstart = crash_memory_range[i].start;
		mend = crash_memory_range[i].end;
		if (cstart < mend && cend > mstart) {
			if (cstart != mstart && cend != mend) {
				/* Split memory region */
				crash_memory_range[i].end = cstart - 1;
				temp_region.start = cend + 1;
				temp_region.end = mend;
				temp_region.type = RANGE_RAM;
				tidx = i+1;
			} else if (cstart != mstart)
				crash_memory_range[i].end = cstart - 1;
			else
				crash_memory_range[i].start = cend + 1;
		}
	}
	/* Insert split memory region, if any. */
	if (tidx >= 0) {
		if (*nr_ranges == max_memory_ranges) {
			/* No space to insert another element. */
			fprintf(stderr, "Error: Number of crash memory ranges"
					" excedeed the max limit\n");
			return -1;
		}
		for (j = (*nr_ranges - 1); j >= tidx; j--)
			crash_memory_range[j+1] = crash_memory_range[j];
		crash_memory_range[tidx].start = temp_region.start;
		crash_memory_range[tidx].end = temp_region.end;
		crash_memory_range[tidx].type = temp_region.type;
		(*nr_ranges)++;
	}
	return 0;
}

static int get_crash_memory_ranges(int *ranges)
{
	const char *iomem = proc_iomem();
        char line[MAX_LINE];
        FILE *fp;
        unsigned long start, end;

	crash_memory_range = xmalloc(sizeof(struct memory_range) *
				max_memory_ranges);
        fp = fopen(iomem, "r");
        if (!fp) {
                fprintf(stderr, "Cannot open %s: %s\n",
                        iomem, strerror(errno));
                return -1;
        }
	while(fgets(line, sizeof(line), fp) != 0) {
		char *str;
		int type, consumed, count;
		if (memory_ranges >= max_memory_ranges)
			break;
		count = sscanf(line, "%lx-%lx : %n",
				&start, &end, &consumed);
		str = line + consumed;
		if (count != 2)
			continue;

		if (memcmp(str, "System RAM\n", 11) == 0) {
			type = RANGE_RAM;
		} else if (memcmp(str, "Crash kernel\n", 13) == 0) {
			/* Reserved memory region. New kernel can
			 * use this region to boot into. */
			crash_reserved_mem.start = start;
			crash_reserved_mem.end = end;
			crash_reserved_mem.type = RANGE_RAM;
			continue;
		}
		else if (memcmp(str, "Kernel code\n", 12) == 0) {
			kernel_code_start = start;
			kernel_code_end = end;
			continue;
		} else if (memcmp(str, "Uncached RAM\n", 13) == 0) {
			type = RANGE_UNCACHED;
		} else {
			continue;
		}
		crash_memory_range[memory_ranges].start = start;
		crash_memory_range[memory_ranges].end = end;
		crash_memory_range[memory_ranges].type = type;
		memory_ranges++;
	}
        fclose(fp);
	if (exclude_crash_reserve_region(&memory_ranges) < 0)
		return -1;
	*ranges = memory_ranges;
	return 0;
}

/*
 * Note that this assignes a malloced pointer to *cmdline,
 * which is likely never freed by the caller
 */
static void
cmdline_add_elfcorehdr(const char **cmdline, unsigned long addr)
{
	char *str;
	char buf[64];
	size_t len;
	sprintf(buf, " elfcorehdr=%ldK", addr/1024);
	len = strlen(*cmdline) + strlen(buf) + 1;
	str = xmalloc(len);
	sprintf(str, "%s%s", *cmdline, buf);
	*cmdline = str;
}

int load_crashdump_segments(struct kexec_info *info, struct mem_ehdr *ehdr,
                            unsigned long max_addr, unsigned long min_base,
			    const char **cmdline)
{
	int nr_ranges;
	unsigned long sz;
	size_t size;
	void *tmp;
	if (info->kexec_flags & KEXEC_ON_CRASH &&
	    get_crash_memory_ranges(&nr_ranges) == 0) {
		int i;

		elf_info.kern_paddr_start = kernel_code_start;
		for (i=0; i < nr_ranges; i++) {
			unsigned long long mstart = crash_memory_range[i].start;
			unsigned long long mend = crash_memory_range[i].end;
			if (!mstart && !mend)
				continue;
			if (kernel_code_start >= mstart &&
			    kernel_code_start < mend) {
				elf_info.kern_vaddr_start = mstart + LOAD_OFFSET;
				break;
			}
		}
		elf_info.kern_size = kernel_code_end - kernel_code_start + 1;
		if (crash_create_elf64_headers(info, &elf_info,
					       crash_memory_range, nr_ranges,
					       &tmp, &sz, EFI_PAGE_SIZE) < 0)
			return -1;

		elfcorehdr = add_buffer(info, tmp, sz, sz, EFI_PAGE_SIZE,
					min_base, max_addr, -1);
		loaded_segments[loaded_segments_num].start = elfcorehdr;
		loaded_segments[loaded_segments_num].end = elfcorehdr + sz;
		loaded_segments_num++;
		cmdline_add_elfcorehdr(cmdline, elfcorehdr);
	}
	add_loaded_segments_info(ehdr);
	size = sizeof(struct loaded_segment) * loaded_segments_num;
	qsort(loaded_segments, loaded_segments_num,
                        sizeof(struct loaded_segment), seg_comp);
        loaded_segments_base = add_buffer(info, loaded_segments,
                        size, size, 16, 0, max_addr, -1);

        elf_rel_set_symbol(&info->rhdr, "__loaded_segments",
                        &loaded_segments_base, sizeof(long));
        elf_rel_set_symbol(&info->rhdr, "__loaded_segments_num",
                         &loaded_segments_num, sizeof(long));
	return 0;
}

int is_crashkernel_mem_reserved(void)
{
	uint64_t start, end;

	return parse_iomem_single("Crash kernel\n", &start,
				  &end) == 0 ?  (start != end) : 0;
}

int get_crash_kernel_load_range(uint64_t *start, uint64_t *end)
{
	return parse_iomem_single("Crash kernel\n", start, end);
}
