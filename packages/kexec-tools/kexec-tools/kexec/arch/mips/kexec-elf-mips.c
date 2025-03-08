/*
 * kexec-elf-mips.c - kexec Elf loader for mips
 * Copyright (C) 2007 Francesco Chiechi, Alessandro Rubini
 * Copyright (C) 2007 Tvblob s.r.l.
 *
 * derived from ../ppc/kexec-elf-ppc.c
 * Copyright (C) 2004 Albert Herranz
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
#include "kexec-mips.h"
#include "crashdump-mips.h"
#include <arch/options.h>
#include "../../fs2dt.h"
#include "../../dt-ops.h"

static const int probe_debug = 0;

#define BOOTLOADER         "kexec"
#define UPSZ(X) _ALIGN_UP(sizeof(X), 4)

#define CMDLINE_PREFIX "kexec "
static char cmdline_buf[COMMAND_LINE_SIZE] = CMDLINE_PREFIX;

/* Adds initrd parameters to command line. */
static int cmdline_add_initrd(char *cmdline, unsigned long addr, char *new_para)
{
	int cmdlen, len;
	char str[30], *ptr;

	ptr = str;
	strcpy(str, new_para);
	ptr += strlen(str);
	ultoa(addr, ptr);
	len = strlen(str);
	cmdlen = strlen(cmdline) + len;
	if (cmdlen > (COMMAND_LINE_SIZE - 1))
		die("Command line overflow\n");
	strcat(cmdline, str);

	return 0;
}

/* add initrd to cmdline to compatible with previous platforms. */
static int patch_initrd_info(char *cmdline, unsigned long base,
			     unsigned long size)
{
	const char cpuinfo[] = "/proc/cpuinfo";
	char line[MAX_LINE];
	FILE *fp;
	unsigned long page_offset = PAGE_OFFSET;

	fp = fopen(cpuinfo, "r");
	if (!fp) {
		fprintf(stderr, "Cannot open %s: %s\n",
		cpuinfo, strerror(errno));
		return -1;
	}
	while (fgets(line, sizeof(line), fp) != 0) {
		if (strncmp(line, "cpu model", 9) == 0) {
			if (strstr(line, "Loongson")) {
				/* LOONGSON64  uses a different page_offset. */
				if (arch_options.core_header_type ==
				    CORE_TYPE_ELF64)
					page_offset = LOONGSON_PAGE_OFFSET;
				cmdline_add_initrd(cmdline,
					     page_offset + base, " rd_start=");
				cmdline_add_initrd(cmdline, size, " rd_size=");
				break;
			}
		}
	}
	fclose(fp);
	return 0;
}

int elf_mips_probe(const char *buf, off_t len)
{
	struct mem_ehdr ehdr;
	int result;
	result = build_elf_exec_info(buf, len, &ehdr, 0);
	if (result < 0) {
		goto out;
	}

	/* Verify the architecuture specific bits */
	if (ehdr.e_machine != EM_MIPS) {
		/* for a different architecture */
		if (probe_debug) {
			fprintf(stderr, "Not for this architecture.\n");
		}
		result = -1;
		goto out;
	}
	result = 0;
 out:
	free_elf_info(&ehdr);
	return result;
}

void elf_mips_usage(void)
{
}

int elf_mips_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info)
{
	struct mem_ehdr ehdr;
	int command_line_len = 0;
	char *crash_cmdline;
	int result;
	unsigned long cmdline_addr;
	size_t i;
	off_t dtb_length;
	char *dtb_buf;
	char *initrd_buf = NULL;
	unsigned long long kernel_addr = 0, kernel_size = 0;
	unsigned long pagesize = getpagesize();

	/* Need to append some command line parameters internally in case of
	 * taking crash dumps.
	 */
	if (info->kexec_flags & KEXEC_ON_CRASH) {
		crash_cmdline = xmalloc(COMMAND_LINE_SIZE);
		memset((void *)crash_cmdline, 0, COMMAND_LINE_SIZE);
	} else
		crash_cmdline = NULL;

	result = build_elf_exec_info(buf, len, &ehdr, 0);
	if (result < 0)
		die("ELF exec parse failed\n");

	/* Read in the PT_LOAD segments and remove CKSEG0 mask from address*/
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct mem_phdr *phdr;
		phdr = &ehdr.e_phdr[i];
		if (phdr->p_type == PT_LOAD) {
			phdr->p_paddr = virt_to_phys(phdr->p_paddr);
			kernel_addr = phdr->p_paddr;
			kernel_size = phdr->p_memsz;
		}
	}

	/* Load the Elf data */
	result = elf_exec_load(&ehdr, info);
	if (result < 0)
		die("ELF exec load failed\n");

	info->entry = (void *)virt_to_phys(ehdr.e_entry);

	if (arch_options.command_line)
		command_line_len = strlen(arch_options.command_line) + 1;

	if (info->kexec_flags & KEXEC_ON_CRASH) {
		result = load_crashdump_segments(info, crash_cmdline,
				0, 0);
		if (result < 0) {
			free(crash_cmdline);
			return -1;
		}
	}

	if (arch_options.command_line)
		strncat(cmdline_buf, arch_options.command_line, command_line_len);
	if (crash_cmdline)
	{
		strncat(cmdline_buf, crash_cmdline,
				sizeof(crash_cmdline) -
				strlen(crash_cmdline) - 1);
		free(crash_cmdline);
	}

	if (info->kexec_flags & KEXEC_ON_CRASH)
		/* In case of crashdump segment[0] is kernel.
		 * Put cmdline just after it. */
		cmdline_addr = (unsigned long)info->segment[0].mem +
				info->segment[0].memsz;
	else
		cmdline_addr = 0;

	/* MIPS systems that have been converted to use device tree
	 * passed through UHI will use commandline in the DTB and
	 * the DTB passed as a separate buffer. Note that
	 * CMDLINE_PREFIX is skipped here intentionally, as it is
	 * used only in the legacy method */

	if (arch_options.dtb_file) {
		dtb_buf = slurp_file(arch_options.dtb_file, &dtb_length);
	} else {
		create_flatten_tree(&dtb_buf, &dtb_length, cmdline_buf + strlen(CMDLINE_PREFIX));
	}

	if (arch_options.initrd_file) {
		initrd_buf = slurp_file(arch_options.initrd_file, &initrd_size);

		/* Create initrd entries in dtb - although at this time
		 * they would not point to the correct location */
		dtb_set_initrd(&dtb_buf, &dtb_length, (off_t)initrd_buf, (off_t)initrd_buf + initrd_size);

		initrd_base = add_buffer(info, initrd_buf, initrd_size,
					initrd_size, sizeof(void *),
					_ALIGN_UP(kernel_addr + kernel_size + dtb_length,
						pagesize), 0x0fffffff, 1);

		/* Now that the buffer for initrd is prepared, update the dtb
		 * with an appropriate location */
		dtb_set_initrd(&dtb_buf, &dtb_length, initrd_base, initrd_base + initrd_size);

		/* Add the initrd parameters to cmdline */
		patch_initrd_info(cmdline_buf, initrd_base, initrd_size);
	}
	/* This is a legacy method for commandline passing used
	 * currently by Octeon CPUs only */
	add_buffer(info, cmdline_buf, sizeof(cmdline_buf),
			sizeof(cmdline_buf), sizeof(void *),
			cmdline_addr, 0x0fffffff, 1);

	add_buffer(info, dtb_buf, dtb_length, dtb_length, 0,
		_ALIGN_UP(kernel_addr + kernel_size, pagesize),
		0x0fffffff, 1);

	return 0;
}

