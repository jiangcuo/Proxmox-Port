/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2008  Magnus Damm
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
#include "crashdump-sh.h"
#include "kexec-sh.h"

int elf_sh_probe(const char *buf, off_t len)
{
	struct mem_ehdr ehdr;
	int result;
	result = build_elf_exec_info(buf, len, &ehdr, 0);
	if (result < 0)
		goto out;

	/* Verify the architecuture specific bits */
	if (ehdr.e_machine != EM_SH) {
		result = -1;
		goto out;
	}

	result = 0;
 out:
	free_elf_info(&ehdr);
	return result;
}

void elf_sh_usage(void)
{
	printf("  --append=STRING       Set the kernel command line to STRING\n"
		);
}

int elf_sh_load(int argc, char **argv, const char *buf, off_t len,
		struct kexec_info *info)
{
	struct mem_ehdr ehdr;
	char *command_line;
	char *modified_cmdline;
	struct mem_sym sym;
	int opt, rc;
	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{ 0, 0, 0, 0 },
	};

	static const char short_options[] = KEXEC_OPT_STR "";

	/*
	 * Parse the command line arguments
	 */
	command_line = modified_cmdline = 0;
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

	/* Need to append some command line parameters internally in case of
	 * taking crash dumps.
	 */
	if (info->kexec_flags & (KEXEC_ON_CRASH | KEXEC_PRESERVE_CONTEXT)) {
		modified_cmdline = xmalloc(COMMAND_LINE_SIZE);
		memset((void *)modified_cmdline, 0, COMMAND_LINE_SIZE);
		if (command_line) {
			strncpy(modified_cmdline, command_line,
						COMMAND_LINE_SIZE);
			modified_cmdline[COMMAND_LINE_SIZE - 1] = '\0';
		}
	}

	/* Load the ELF executable */
	elf_exec_build_load(info, &ehdr, buf, len, 0);
	info->entry = (void *)virt_to_phys(ehdr.e_entry);

	/* If panic kernel is being loaded, additional segments need
	 * to be created. */
	if (info->kexec_flags & (KEXEC_ON_CRASH | KEXEC_PRESERVE_CONTEXT)) {
		rc = load_crashdump_segments(info, modified_cmdline);
		if (rc < 0)
			return -1;
		/* Use new command line. */
		command_line = modified_cmdline;
	}

	/* If we're booting a vmlinux then fill in empty_zero_page */
	if (elf_rel_find_symbol(&ehdr, "empty_zero_page", &sym) == 0) {
		char *zp = (void *)ehdr.e_shdr[sym.st_shndx].sh_data;

		kexec_sh_setup_zero_page(zp, 4096, command_line);
	}

	return 0;
}
