/*
 * kexec-sh.c - kexec for the SH
 * Copyright (C) 2004 kogiidena@eggplant.ddo.jp
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
#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "kexec-sh.h"
#include <arch/options.h>

#define MAX_MEMORY_RANGES 64
static struct memory_range memory_range[MAX_MEMORY_RANGES];

static int kexec_sh_memory_range_callback(void *UNUSED(data), int nr,
					  char *UNUSED(str),
					  unsigned long long base,
					  unsigned long long length)
{
	if (nr < MAX_MEMORY_RANGES) {
		memory_range[nr].start = base;
		memory_range[nr].end = base + length - 1;
		memory_range[nr].type = RANGE_RAM;
		return 0;
	}

	return 1;
}

/* Return a sorted list of available memory ranges. */
int get_memory_ranges(struct memory_range **range, int *ranges,
		      unsigned long kexec_flags)
{
	int nr, ret;
	nr = kexec_iomem_for_each_line("System RAM\n",
				       kexec_sh_memory_range_callback, NULL);
	*range = memory_range;
	*ranges = nr;

	/*
	 * Redefine the memory region boundaries if kernel
	 * exports the limits and if it is panic kernel.
	 * Override user values only if kernel exported values are
	 * subset of user defined values.
	 */
	if (kexec_flags & KEXEC_ON_CRASH) {
		unsigned long long start, end;

		ret = parse_iomem_single("Crash kernel\n", &start, &end);
		if (ret != 0) {
			fprintf(stderr, "parse_iomem_single failed.\n");
			return -1;
		}

		if (start > mem_min)
			mem_min = start;
		if (end < mem_max)
			mem_max = end;
	}

	return 0;
}

/* Supported file types and callbacks */
struct file_type file_type[] = {
	/* uImage is probed before zImage because the latter also accepts
	   uncompressed images. */
	{ "uImage-sh", uImage_sh_probe, uImage_sh_load, zImage_sh_usage },
	{ "zImage-sh", zImage_sh_probe, zImage_sh_load, zImage_sh_usage },
	{ "elf-sh", elf_sh_probe, elf_sh_load, elf_sh_usage },
	{ "netbsd-sh", netbsd_sh_probe, netbsd_sh_load, netbsd_sh_usage },
};
int file_types = sizeof(file_type) / sizeof(file_type[0]);


void arch_usage(void)
{

  printf(
    " none\n\n"
    "Default options:\n"
    " --append=\"%s\"\n"
    " STRING of --append is set from /proc/cmdline as default.\n"
    ,get_append());

}

int arch_process_options(int argc, char **argv)
{
	/* The common options amongst loaders (e.g. --append) should be read
	 * here, and the loader-specific options (e.g. NetBSD stuff) should
	 * then be re-parsed in the loader.
	 * (e.g. in kexec-netbsd-sh.c, for example.)
	 */
	static const struct option options[] = {
		KEXEC_ALL_OPTIONS
		{ 0, 			0, NULL, 0 },
	};
	static const char short_options[] = KEXEC_ARCH_OPT_STR;
	int opt;

	opterr = 0; /* Don't complain about unrecognized options here */
	while((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch(opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_MAX) {
				break;
			}
		case OPT_APPEND:
		case OPT_NBSD_HOWTO:
		case OPT_NBSD_MROOT:
		  ;
		}
	}
	/* Reset getopt for the next pass; called in other source modules */
	opterr = 1;
	optind = 1;
	return 0;
}

const struct arch_map_entry arches[] = {
	/* For compatibility with older patches
	 * use KEXEC_ARCH_DEFAULT instead of KEXEC_ARCH_SH here.
	 */
	{ "sh3", KEXEC_ARCH_DEFAULT },
	{ "sh4", KEXEC_ARCH_DEFAULT },
	{ "sh4a", KEXEC_ARCH_DEFAULT },
	{ "sh4al-dsp", KEXEC_ARCH_DEFAULT },
	{ NULL, 0 },
};

int arch_compat_trampoline(struct kexec_info *UNUSED(info))
{
	return 0;
}

void arch_update_purgatory(struct kexec_info *UNUSED(info))
{
}

char append_buf[256];

char *get_append(void)
{
        FILE *fp;
        int len;
        if((fp = fopen("/proc/cmdline", "r")) == NULL){
              die("/proc/cmdline file open error !!\n");
        }
        fgets(append_buf, 256, fp);
        len = strlen(append_buf);
        append_buf[len-1] = 0;
        fclose(fp);
        return append_buf;
}

void kexec_sh_setup_zero_page(char *zero_page_buf, size_t zero_page_size,
			      char *cmd_line)
{
	size_t n = zero_page_size - 0x100;

	memset(zero_page_buf, 0, zero_page_size);

	if (cmd_line) {
		if (n > strlen(cmd_line))
			n = strlen(cmd_line);

		memcpy(zero_page_buf + 0x100, cmd_line, n);
		zero_page_buf[0x100 + n] = '\0';
	}
}

static int is_32bit(void)
{
	const char *cpuinfo = "/proc/cpuinfo";
	char line[MAX_LINE];
	FILE *fp;
	int status = 0;

	fp = fopen(cpuinfo, "r");
	if (!fp)
		die("Cannot open %s\n", cpuinfo);

	while(fgets(line, sizeof(line), fp) != 0) {
		const char *key = "address sizes";
		const char *value = " 32 bits physical";
		char *p;
		if (strncmp(line, key, strlen(key)))
			continue;
		p = strchr(line + strlen(key), ':');
		if (!p)
			continue;
		if (!strncmp(p + 1, value, strlen(value)))
			status = 1;
		break;
	}

	fclose(fp);

	return status;
}

unsigned long virt_to_phys(unsigned long addr)
{
	unsigned long seg = addr & 0xe0000000;
	unsigned long long start = 0;
	int have_32bit = is_32bit();

	if (seg != 0x80000000 && (have_32bit || seg != 0xc0000000))
		die("Virtual address %p is not in P1%s\n", (void *)addr,
		    have_32bit ? "" : " or P2");

	/* If 32bit addressing is used then the base of system RAM
	 * is an offset into physical memory. */
	if (have_32bit) {
		unsigned long long end;
		int ret;

		/* Assume there is only one "System RAM" region */
		ret = parse_iomem_single("System RAM\n", &start, &end);
		if (ret)
			die("Could not parse System RAM region "
			    "in /proc/iomem\n");
	}

	return addr - seg + start;
}

/*
 * add_segment() should convert base to a physical address on superh,
 * while the default is just to work with base as is */
void add_segment(struct kexec_info *info, const void *buf, size_t bufsz,
		 unsigned long base, size_t memsz)
{
	add_segment_phys_virt(info, buf, bufsz, base, memsz, 1);
}

/*
 * add_buffer() should convert base to a physical address on superh,
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
