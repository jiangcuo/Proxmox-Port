/*
 * kexec-elf-m68k.c - kexec Elf loader for m68k
 *
 * Copyright (C) 2013 Geert Uytterhoeven
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <elf.h>
#include <boot/elf_boot.h>
#include <ip_checksum.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"
#include "../../kexec-syscall.h"
#include "kexec-m68k.h"
#include "bootinfo.h"
#include <arch/options.h>

#define KiB		* 1024
#define MiB		* 1024 KiB

#define PAGE_SIZE	4 KiB


int elf_m68k_probe(const char *buf, off_t len)
{
	struct mem_ehdr ehdr;
	int result;
	result = build_elf_exec_info(buf, len, &ehdr, 0);
	if (result < 0)
		goto out;

	/* Verify the architecuture specific bits */
	if (ehdr.e_machine != EM_68K) {
		/* for a different architecture */
		fprintf(stderr, "Not for this architecture.\n");
		result = -1;
		goto out;
	}
	result = 0;
 out:
	free_elf_info(&ehdr);
	return result;
}

void elf_m68k_usage(void)
{
	printf("    --command-line=STRING Set the kernel command line to STRING\n"
	       "    --append=STRING       Set the kernel command line to STRING\n"
	       "    --reuse-cmdline       Use kernel command line from running system.\n"
	       "    --ramdisk=FILE        Use FILE as the kernel's initial ramdisk.\n"
	       "    --initrd=FILE         Use FILE as the kernel's initial ramdisk.\n"
	       "    --bootinfo=FILE       Use FILE as the kernel's bootinfo\n"
	       );
}

static unsigned long segment_end(const struct kexec_info *info, int i)
{
	return (unsigned long)info->segment[i].mem + info->segment[i].memsz - 1;
}

int elf_m68k_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info)
{
	struct mem_ehdr ehdr;
	const char *cmdline = NULL, *ramdisk_file = NULL;
	int opt, result, i;
	unsigned long bootinfo_addr, ramdisk_addr = 0;
	off_t ramdisk_size = 0;

	/* See options.h if adding any more options. */
	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{ "command-line",	1, NULL, OPT_APPEND },
		{ "append",		1, NULL, OPT_APPEND },
		{ "reuse-cmdline",	0, NULL, OPT_REUSE_CMDLINE },
		{ "ramdisk",		1, NULL, OPT_RAMDISK },
		{ "initrd",		1, NULL, OPT_RAMDISK },
		{ "bootinfo",		1, NULL, OPT_BOOTINFO },
		{ 0,                    0, NULL, 0 },
	};

	static const char short_options[] = KEXEC_ARCH_OPT_STR "d";

	while ((opt = getopt_long(argc, argv, short_options, options, 0)) !=
		-1) {
		switch (opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX)
				break;
		case OPT_APPEND:
			cmdline = optarg;
			break;
		case OPT_REUSE_CMDLINE:
			cmdline = get_command_line();
			break;
		case OPT_RAMDISK:
			ramdisk_file = optarg;
			break;
		case OPT_BOOTINFO:
			break;
		}
	}

	result = build_elf_exec_info(buf, len, &ehdr, 0);
	if (result < 0)
		die("ELF exec parse failed\n");

	/* Fixup PT_LOAD segments that include the ELF header (offset zero) */
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct mem_phdr *phdr;
		phdr = &ehdr.e_phdr[i];
		if (phdr->p_type != PT_LOAD || phdr->p_offset)
			continue;

		dbgprintf("Removing ELF header from segment %d\n", i);
		phdr->p_paddr += PAGE_SIZE;
		phdr->p_vaddr += PAGE_SIZE;
		phdr->p_filesz -= PAGE_SIZE;
		phdr->p_memsz -= PAGE_SIZE;
		phdr->p_offset += PAGE_SIZE;
		phdr->p_data += PAGE_SIZE;
	}

	/* Load the ELF data */
	result = elf_exec_load(&ehdr, info);
	if (result < 0)
		die("ELF exec load failed\n");

	info->entry = (void *)virt_to_phys(ehdr.e_entry);

	/* Bootinfo must be stored right after the kernel */
	bootinfo_addr = segment_end(info, info->nr_segments - 1) + 1;

	/* Load ramdisk */
	if (ramdisk_file) {
		void *ramdisk = slurp_decompress_file(ramdisk_file,
						      &ramdisk_size);
		/* Store ramdisk at top of first memory chunk */
		ramdisk_addr = _ALIGN_DOWN(info->memory_range[0].end -
					   ramdisk_size + 1,
					   PAGE_SIZE);
		if (!buf)
			die("Ramdisk load failed\n");
		add_buffer(info, ramdisk, ramdisk_size, ramdisk_size,
			   PAGE_SIZE, ramdisk_addr, info->memory_range[0].end,
			   1);
	}

	/* Update and add bootinfo */
	bootinfo_set_cmdline(cmdline);
	bootinfo_set_ramdisk(ramdisk_addr, ramdisk_size);
	bootinfo_add_rng_seed();
	if (kexec_debug)
		bootinfo_print();
	add_bootinfo(info, bootinfo_addr);

	/*
	 * Check if the kernel (and bootinfo) exceed 4 MiB, as current kernels
	 * don't support that.
	 * As the segments are still unsorted, the bootinfo is located in the
	 * last segment.
	 */
	if (segment_end(info, info->nr_segments - 1) >= virt_to_phys(4 MiB - 1))
		printf("WARNING: Kernel is larger than 4 MiB\n");

	/* Check struct bootversion at start of kernel */
	bootinfo_check_bootversion(info);

	return 0;
}
