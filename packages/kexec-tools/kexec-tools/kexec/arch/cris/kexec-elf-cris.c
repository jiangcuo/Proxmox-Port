/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2008  AXIS Communications AB
 * Written by Edgar E. Iglesias
 *
 * Based on x86 implementation,
 * Copyright (C) 2003-2005  Eric Biederman (ebiederm@xmission.com)
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <elf.h>
#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "../../kexec-elf.h"
#include "../../kexec-elf-boot.h"
#include <arch/options.h>
#include "kexec-cris.h"

int elf_cris_probe(const char *buf, off_t len)
{
	struct mem_ehdr ehdr;
	int result;
	result = build_elf_exec_info(buf, len, &ehdr, 0);
	if (result < 0)
		goto out;

	/* Verify the architecuture specific bits */
	if (ehdr.e_machine != EM_CRIS) {
		result = -1;
		goto out;
	}

	result = 0;
 out:
	free_elf_info(&ehdr);
	return result;
}

void elf_cris_usage(void)
{
	printf("  --append=STRING       Set the kernel command line to STRING\n"
		);
}

#define CRAMFS_MAGIC 0x28cd3d45
#define JHEAD_MAGIC 0x1FF528A6
#define JHEAD_SIZE 8
#define RAM_INIT_MAGIC 0x56902387
#define COMMAND_LINE_MAGIC 0x87109563
#define NAND_BOOT_MAGIC 0x9a9db001

int elf_cris_load(int argc, char **argv, const char *buf, off_t len,
		struct kexec_info *info)
{
	struct mem_ehdr ehdr;
	char *command_line;
	unsigned int *trampoline_buf;
	unsigned long trampoline_base;
	int opt;
	extern void cris_trampoline(void);
	extern unsigned long cris_trampoline_size;
	extern struct regframe_t {
		unsigned int regs[16];
	} cris_regframe;

	/* See options.h -- add any more there, too. */
	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{"append", 1, 0, OPT_APPEND},
		{ 0, 0, 0, 0 },
	};

	static const char short_options[] = KEXEC_OPT_STR "";

	/*
	 * Parse the command line arguments
	 */
	command_line = 0;
	while((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch(opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX) {
				break;
			}
		case OPT_APPEND:
			command_line = optarg;
			break;
		}
	}

	/* Load the ELF executable */
	elf_exec_build_load(info, &ehdr, buf, len, 0);

	cris_regframe.regs[0] = virt_to_phys(ehdr.e_entry);
	cris_regframe.regs[8] = RAM_INIT_MAGIC;
	cris_regframe.regs[12] = NAND_BOOT_MAGIC;

	trampoline_buf = xmalloc(cris_trampoline_size);
	trampoline_base = add_buffer_virt(info,
					  trampoline_buf,
					  cris_trampoline_size,
					  cris_trampoline_size,
					  4, 0, elf_max_addr(&ehdr), 1);
	memcpy(trampoline_buf,
	       cris_trampoline, cris_trampoline_size);
	info->entry = (void *)trampoline_base;
	return 0;
}
