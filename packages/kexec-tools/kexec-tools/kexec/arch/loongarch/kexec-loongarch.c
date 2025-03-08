/*
 * kexec-loongarch.c - kexec for loongarch
 *
 * Copyright (C) 2022 Loongson Technology Corporation Limited.
 *   Youling Tang <tangyouling@loongson.cn>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <linux/elf-em.h>
#include <elf.h>
#include <elf_info.h>

#include "kexec.h"
#include "kexec-loongarch.h"
#include "crashdump-loongarch.h"
#include "iomem.h"
#include "kexec-syscall.h"
#include "mem_regions.h"
#include "arch/options.h"

#define CMDLINE_PREFIX "kexec "
static char cmdline[COMMAND_LINE_SIZE] = CMDLINE_PREFIX;

/* Adds "initrd=start,size" parameters to command line. */
static int cmdline_add_initrd(char *cmdline, unsigned long addr,
		unsigned long size)
{
	int cmdlen, len;
	char str[50], *ptr;

	ptr = str;
	strcpy(str, " initrd=");
	ptr += strlen(str);
	ultoa(addr, ptr);
	strcat(str, ",");
	ptr = str + strlen(str);
	ultoa(size, ptr);
	len = strlen(str);
	cmdlen = strlen(cmdline) + len;
	if (cmdlen > (COMMAND_LINE_SIZE - 1))
		die("Command line overflow\n");
	strcat(cmdline, str);

	return 0;
}

/* Adds the appropriate "mem=size@start" options to command line, indicating the
 * memory region the new kernel can use to boot into. */
static int cmdline_add_mem(char *cmdline, unsigned long addr,
		unsigned long size)
{
	int cmdlen, len;
	char str[50], *ptr;

	addr = addr/1024;
	size = size/1024;
	ptr = str;
	strcpy(str, " mem=");
	ptr += strlen(str);
	ultoa(size, ptr);
	strcat(str, "K@");
	ptr = str + strlen(str);
	ultoa(addr, ptr);
	strcat(str, "K");
	len = strlen(str);
	cmdlen = strlen(cmdline) + len;
	if (cmdlen > (COMMAND_LINE_SIZE - 1))
		die("Command line overflow\n");
	strcat(cmdline, str);

	return 0;
}

/* Adds the "elfcorehdr=size@start" command line parameter to command line. */
static int cmdline_add_elfcorehdr(char *cmdline, unsigned long addr,
			unsigned long size)
{
	int cmdlen, len;
	char str[50], *ptr;

	addr = addr/1024;
	size = size/1024;
	ptr = str;
	strcpy(str, " elfcorehdr=");
	ptr += strlen(str);
	ultoa(size, ptr);
	strcat(str, "K@");
	ptr = str + strlen(str);
	ultoa(addr, ptr);
	strcat(str, "K");
	len = strlen(str);
	cmdlen = strlen(cmdline) + len;
	if (cmdlen > (COMMAND_LINE_SIZE - 1))
		die("Command line overflow\n");
	strcat(cmdline, str);

	return 0;
}

/* Return a sorted list of memory ranges. */
static struct memory_range memory_range[MAX_MEMORY_RANGES];

int get_memory_ranges(struct memory_range **range, int *ranges,
		      unsigned long UNUSED(kexec_flags))
{
	int memory_ranges = 0;

	const char *iomem = proc_iomem();
	char line[MAX_LINE];
	FILE *fp;
	unsigned long long start, end;
	char *str;
	int type, consumed, count;

	fp = fopen(iomem, "r");
	if (!fp) {
		fprintf(stderr, "Cannot open %s: %s\n", iomem, strerror(errno));
		return -1;
	}

	while (fgets(line, sizeof(line), fp) != 0) {
		if (memory_ranges >= MAX_MEMORY_RANGES)
			break;
		count = sscanf(line, "%llx-%llx : %n", &start, &end, &consumed);
		if (count != 2)
			continue;
		str = line + consumed;
		end = end + 1;
		if (!strncmp(str, SYSTEM_RAM, strlen(SYSTEM_RAM)))
			type = RANGE_RAM;
		else if (!strncmp(str, IOMEM_RESERVED, strlen(IOMEM_RESERVED)))
			type = RANGE_RESERVED;
		else
			continue;

		if (memory_ranges > 0 &&
		    memory_range[memory_ranges - 1].end == start &&
		    memory_range[memory_ranges - 1].type == type) {
			memory_range[memory_ranges - 1].end = end;
		} else {
			memory_range[memory_ranges].start = start;
			memory_range[memory_ranges].end = end;
			memory_range[memory_ranges].type = type;
			memory_ranges++;
		}
	}
	fclose(fp);
	*range = memory_range;
	*ranges = memory_ranges;

	dbgprint_mem_range("MEMORY RANGES:", *range, *ranges);
	return 0;
}

struct file_type file_type[] = {
	{"elf-loongarch", elf_loongarch_probe, elf_loongarch_load, elf_loongarch_usage},
	{"pez-loongarch", pez_loongarch_probe, pez_loongarch_load, pez_loongarch_usage},
	{"pei-loongarch", pei_loongarch_probe, pei_loongarch_load, pei_loongarch_usage},
};
int file_types = sizeof(file_type) / sizeof(file_type[0]);

/* loongarch global varables. */

struct loongarch_mem loongarch_mem;

/**
 * loongarch_process_image_header - Process the loongarch image header.
 */

int loongarch_process_image_header(const struct loongarch_image_header *h)
{

	if (!loongarch_header_check_pe_sig(h))
		return EFAILED;

	if (h->image_size) {
		loongarch_mem.text_offset = loongarch_header_text_offset(h);
		loongarch_mem.image_size = loongarch_header_image_size(h);
	}

	return 0;
}

void arch_usage(void)
{
	printf(loongarch_opts_usage);
}

struct arch_options_t arch_options = {
	.core_header_type = CORE_TYPE_ELF64,
};

int arch_process_options(int argc, char **argv)
{
	static const char short_options[] = KEXEC_ARCH_OPT_STR "";
	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{ 0 },
	};
	int opt;
	char *cmdline = NULL;
	const char *append = NULL;

	while ((opt = getopt_long(argc, argv, short_options,
				  options, 0)) != -1) {
		switch (opt) {
		case OPT_APPEND:
			append = optarg;
			break;
		case OPT_REUSE_CMDLINE:
			cmdline = get_command_line();
			remove_parameter(cmdline, "kexec");
			remove_parameter(cmdline, "initrd");
			break;
		case OPT_INITRD:
			arch_options.initrd_file = optarg;
			break;
		default:
			break;
		}
	}

	arch_options.command_line = concat_cmdline(cmdline, append);

	dbgprintf("%s:%d: command_line: %s\n", __func__, __LINE__,
		arch_options.command_line);
	dbgprintf("%s:%d: initrd: %s\n", __func__, __LINE__,
		arch_options.initrd_file);

	return 0;
}

const struct arch_map_entry arches[] = {
	{ "loongarch64", KEXEC_ARCH_LOONGARCH },
	{ NULL, 0 },
};

unsigned long loongarch_locate_kernel_segment(struct kexec_info *info)
{
	unsigned long hole;

	if (info->kexec_flags & KEXEC_ON_CRASH) {
		unsigned long hole_end;

		hole = (crash_reserved_mem[usablemem_rgns.size - 1].start < mem_min ?
				mem_min : crash_reserved_mem[usablemem_rgns.size - 1].start) +
				loongarch_mem.text_offset;
		hole = _ALIGN_UP(hole, MiB(1));
		hole_end = hole + loongarch_mem.text_offset + loongarch_mem.image_size;

		if ((hole_end > mem_max) ||
		    (hole_end > crash_reserved_mem[usablemem_rgns.size - 1].end)) {
			dbgprintf("%s: Crash kernel out of range\n", __func__);
			hole = ULONG_MAX;
		}
	} else {
		unsigned long hole_min;
		unsigned long hole_max;

		hole_min = loongarch_mem.text_offset;
		hole_max = hole_min + loongarch_mem.image_size;
		hole = locate_hole(info, loongarch_mem.image_size,
			MiB(1), hole_min, hole_max, 1);

		if (hole == ULONG_MAX)
			dbgprintf("%s: locate_hole failed\n", __func__);
	}

	return hole;
}

/*
 * loongarch_load_other_segments - Prepare the initrd and cmdline segments.
 */

int loongarch_load_other_segments(struct kexec_info *info, unsigned long hole_min)
{
	unsigned long initrd_min, hole_max;
	char *initrd_buf = NULL;
	unsigned long pagesize = getpagesize();
	int i;

	if (arch_options.command_line) {
		if (strlen(arch_options.command_line) >
		    sizeof(cmdline) - 1) {
			fprintf(stderr,
				"Kernel command line too long for kernel!\n");
			return EFAILED;
		}

		strncat(cmdline, arch_options.command_line, sizeof(cmdline) - 1);
	}

	/* Put the other segments after the image. */

	initrd_min = hole_min;
	if (info->kexec_flags & KEXEC_ON_CRASH)
		hole_max = crash_reserved_mem[usablemem_rgns.size - 1].end;
	else
		hole_max = ULONG_MAX;

	if (arch_options.initrd_file) {

		initrd_buf = slurp_decompress_file(arch_options.initrd_file, &initrd_size);

		initrd_base = add_buffer(info, initrd_buf, initrd_size,
					initrd_size, sizeof(void *),
					_ALIGN_UP(initrd_min,
						pagesize), hole_max, 1);
		dbgprintf("initrd_base: %lx, initrd_size: %lx\n", initrd_base, initrd_size);

		cmdline_add_initrd(cmdline, initrd_base, initrd_size);
	}

	if (info->kexec_flags & KEXEC_ON_CRASH) {
		cmdline_add_elfcorehdr(cmdline, elfcorehdr_mem.start,
				elfcorehdr_mem.end - elfcorehdr_mem.start + 1);

		for(i = 0;i < usablemem_rgns.size; i++) {
			cmdline_add_mem(cmdline, crash_reserved_mem[i].start,
			crash_reserved_mem[i].end -
			crash_reserved_mem[i].start + 1);
		}
	}

	cmdline[sizeof(cmdline) - 1] = 0;
	add_buffer(info, cmdline, sizeof(cmdline), sizeof(cmdline),
		sizeof(void *), _ALIGN_UP(hole_min, getpagesize()),
		hole_max, 1);

	dbgprintf("%s:%d: command_line: %s\n", __func__, __LINE__, cmdline);

	return 0;

}

int arch_compat_trampoline(struct kexec_info *UNUSED(info))
{
	return 0;
}

void arch_update_purgatory(struct kexec_info *UNUSED(info))
{
}

unsigned long virt_to_phys(unsigned long addr)
{
	return addr & ((1ULL << 48) - 1);
}

/*
 * add_segment() should convert base to a physical address on loongarch,
 * while the default is just to work with base as is
 */
void add_segment(struct kexec_info *info, const void *buf, size_t bufsz,
		 unsigned long base, size_t memsz)
{
	add_segment_phys_virt(info, buf, bufsz, virt_to_phys(base), memsz, 1);
}

/*
 * add_buffer() should convert base to a physical address on loongarch,
 * while the default is just to work with base as is
 */
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
