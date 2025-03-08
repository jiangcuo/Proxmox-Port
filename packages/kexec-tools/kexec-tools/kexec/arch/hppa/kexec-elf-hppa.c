/*
 * kexec-elf-hppa.c - kexec Elf loader for hppa
 *
 * Copyright (c) 2019 Sven Schnelle <svens@stackframe.org>
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
#include "kexec-hppa.h"
#include <arch/options.h>

#define PAGE_SIZE	4096

extern unsigned long phys_offset;

int elf_hppa_probe(const char *buf, off_t len)
{
	struct mem_ehdr ehdr;
	int result;
	result = build_elf_exec_info(buf, len, &ehdr, 0);
	if (result < 0)
		goto out;

	phys_offset = ehdr.e_entry & 0xf0000000;
	/* Verify the architecuture specific bits */
	if (ehdr.e_machine != EM_PARISC) {
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

void elf_hppa_usage(void)
{
	printf("    --command-line=STRING Set the kernel command line to STRING\n"
	       "    --append=STRING       Set the kernel command line to STRING\n"
	       "    --reuse-cmdline       Use kernel command line from running system.\n"
	       "    --ramdisk=FILE        Use FILE as the kernel's initial ramdisk.\n"
	       "    --initrd=FILE         Use FILE as the kernel's initial ramdisk.\n"
	       );
}

int elf_hppa_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info)
{
	struct mem_ehdr ehdr;
	char *cmdline = NULL, *ramdisk = NULL;
	int opt, result, i;
	unsigned long ramdisk_addr = 0;
	off_t ramdisk_size = 0;

	static const struct option options[] = {
		KEXEC_ALL_OPTIONS
		{ 0,                    0, NULL, 0 },
	};

	static const char short_options[] = KEXEC_ALL_OPT_STR;

	while ((opt = getopt_long(argc, argv, short_options, options, 0)) !=
		-1) {
		switch (opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX)
				break;
		case OPT_APPEND:
			cmdline = strdup(optarg);
			break;
		case OPT_REUSE_CMDLINE:
			cmdline = get_command_line();
			break;
		case OPT_RAMDISK:
			ramdisk = optarg;
			break;
		}
	}

	if (info->file_mode) {
		if (cmdline) {
			info->command_line = cmdline;
			info->command_line_len = strlen(cmdline) + 1;
		}

		if (ramdisk) {
			info->initrd_fd = open(ramdisk, O_RDONLY);
			if (info->initrd_fd == -1) {
				fprintf(stderr, "Could not open initrd file "
					"%s:%s\n", ramdisk, strerror(errno));
				return -1;
			}
		}
		return 0;
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


	/* Load ramdisk */
	if (ramdisk) {
		void *initrd = slurp_decompress_file(ramdisk, &ramdisk_size);
		/* Store ramdisk at top of first memory chunk */
		ramdisk_addr = _ALIGN_DOWN(info->memory_range[0].end -
					   ramdisk_size + 1, PAGE_SIZE);
		if (!buf)
			die("Ramdisk load failed\n");
		add_buffer(info, initrd, ramdisk_size, ramdisk_size,
			   PAGE_SIZE, ramdisk_addr, info->memory_range[0].end,
			   1);
	}

	return 0;
}
