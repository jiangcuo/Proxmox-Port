/*
 * kexec: Linux boots Linux
 *
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

#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <getopt.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"
#include "../../kexec-syscall.h"
#include "../../firmware_memmap.h"
#include "kexec-x86.h"
#include "crashdump-x86.h"
#include <arch/options.h>

struct file_type file_type[] = {
	{ "multiboot-x86", multiboot_x86_probe, multiboot_x86_load,
	  multiboot_x86_usage },
	{ "multiboot2-x86", multiboot2_x86_probe, multiboot2_x86_load,
	  multiboot2_x86_usage },
	{ "elf-x86", elf_x86_probe, elf_x86_load, elf_x86_usage },
	{ "bzImage", bzImage_probe, bzImage_load, bzImage_usage },
	{ "beoboot-x86", beoboot_probe, beoboot_load, beoboot_usage },
	{ "nbi-x86", nbi_probe, nbi_load, nbi_usage },
};
int file_types = sizeof(file_type)/sizeof(file_type[0]);


void arch_usage(void)
{
	printf(
		"     --reset-vga               Attempt to reset a standard vga device\n"
		"     --serial=<port>           Specify the serial port for debug output\n"
		"     --serial-baud=<baud_rate> Specify the serial port baud rate\n"
		"     --console-vga             Enable the vga console\n"
		"     --console-serial          Enable the serial console\n"
		"     --elf32-core-headers      Prepare core headers in ELF32 format\n"
		"     --elf64-core-headers      Prepare core headers in ELF64 format\n"
		"     --pass-memmap-cmdline     Pass memory map via command line in kexec on panic case\n"
		"     --noefi                   Disable efi support\n"
		);
}

struct arch_options_t arch_options = {
	.reset_vga   = 0,
	.serial_base = 0x3f8,
	.serial_baud = 0,
	.console_vga = 0,
	.console_serial = 0,
	.core_header_type = CORE_TYPE_UNDEF,
	.pass_memmap_cmdline = 0,
	.noefi = 0,
};

int arch_process_options(int argc, char **argv)
{
	static const struct option options[] = {
		KEXEC_ALL_OPTIONS
		{ 0, 			0, NULL, 0 },
	};
	static const char short_options[] = KEXEC_ALL_OPT_STR;
	int opt;
	unsigned long value;
	char *end;

	opterr = 0; /* Don't complain about unrecognized options here */
	while((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch(opt) {
		default:
			break;
		case OPT_RESET_VGA:
			arch_options.reset_vga = 1;
			break;
		case OPT_CONSOLE_VGA:
			arch_options.console_vga = 1;
			break;
		case OPT_CONSOLE_SERIAL:
			arch_options.console_serial = 1;
			break;
		case OPT_SERIAL:
			value = ULONG_MAX;
			if (strcmp(optarg, "ttyS0") == 0) {
				value = 0x3f8;
			}
			else if (strcmp(optarg, "ttyS1") == 0) {
				value = 0x2f8;
			}
			else if (strncmp(optarg, "0x", 2) == 0) {
				value = strtoul(optarg +2, &end, 16);
				if (*end != '\0') {
					value = ULONG_MAX;
				}
			}
			if (value >= 65536) {
				fprintf(stderr, "Bad serial port base '%s'\n",
					optarg);
				usage();
				return -1;

			}
			arch_options.serial_base = value;
			break;
		case OPT_SERIAL_BAUD:
			value = strtoul(optarg, &end, 0);
			if ((value > 115200) || ((115200 %value) != 0) ||
				(value < 9600) || (*end))
			{
				fprintf(stderr, "Bad serial port baud rate '%s'\n",
					optarg);
				usage();
				return -1;

			}
			arch_options.serial_baud = value;
			break;
		case OPT_ELF32_CORE:
			arch_options.core_header_type = CORE_TYPE_ELF32;
			break;
		case OPT_ELF64_CORE:
			arch_options.core_header_type = CORE_TYPE_ELF64;
			break;
		case OPT_PASS_MEMMAP_CMDLINE:
			arch_options.pass_memmap_cmdline = 1;
			break;
		case OPT_NOEFI:
			arch_options.noefi = 1;
			break;
		}
	}
	/* Reset getopt for the next pass; called in other source modules */
	opterr = 1;
	optind = 1;
	return 0;
}

const struct arch_map_entry arches[] = {
	/* For compatibility with older patches
	 * use KEXEC_ARCH_DEFAULT instead of KEXEC_ARCH_386 here.
	 */
	{ "i386",   KEXEC_ARCH_DEFAULT },
	{ "i486",   KEXEC_ARCH_DEFAULT },
	{ "i586",   KEXEC_ARCH_DEFAULT },
	{ "i686",   KEXEC_ARCH_DEFAULT },
	{ "x86_64", KEXEC_ARCH_X86_64  },
	{ 0,        0  		       },
};

int arch_compat_trampoline(struct kexec_info *info)
{
	if ((info->kexec_flags & KEXEC_ARCH_MASK) == KEXEC_ARCH_X86_64)
	{
		if (!info->rhdr.e_shdr) {
			fprintf(stderr,
				"A trampoline is required for cross architecture support\n");
			return -1;
		}
		elf_rel_set_symbol(&info->rhdr, "compat_x86_64_entry32",
			&info->entry, sizeof(info->entry));

		info->entry = (void *)elf_rel_get_addr(&info->rhdr, "compat_x86_64");
	}
	return 0;
}

void arch_update_purgatory(struct kexec_info *info)
{
	uint8_t panic_kernel = 0;

	elf_rel_set_symbol(&info->rhdr, "reset_vga",
		&arch_options.reset_vga, sizeof(arch_options.reset_vga));
	elf_rel_set_symbol(&info->rhdr, "serial_base",
		&arch_options.serial_base, sizeof(arch_options.serial_base));
	elf_rel_set_symbol(&info->rhdr, "serial_baud",
		&arch_options.serial_baud, sizeof(arch_options.serial_baud));
	elf_rel_set_symbol(&info->rhdr, "console_vga",
		&arch_options.console_vga, sizeof(arch_options.console_vga));
	elf_rel_set_symbol(&info->rhdr, "console_serial",
		&arch_options.console_serial, sizeof(arch_options.console_serial));
	elf_rel_set_symbol(&info->rhdr, "backup_src_start",
		&info->backup_src_start, sizeof(info->backup_src_start));
	elf_rel_set_symbol(&info->rhdr, "backup_src_size",
		&info->backup_src_size, sizeof(info->backup_src_size));
	if (info->kexec_flags & KEXEC_ON_CRASH) {
		panic_kernel = 1;
		elf_rel_set_symbol(&info->rhdr, "backup_start",
				&info->backup_start, sizeof(info->backup_start));
	}
	elf_rel_set_symbol(&info->rhdr, "panic_kernel",
		&panic_kernel, sizeof(panic_kernel));
}

int arch_do_exclude_segment(struct kexec_info *info, struct kexec_segment *segment)
{
	if (info->elfcorehdr == (unsigned long) segment->mem)
		return 1;

	return 0;
}
