/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2003,2004  Eric Biederman (ebiederm@xmission.com)
 * Copyright (C) 2004 Albert Herranz
 * Copyright (C) 2004 Silicon Graphics, Inc.
 *   Jesse Barnes <jbarnes@sgi.com>
 * Copyright (C) 2004 Khalid Aziz <khalid.aziz@hp.com> Hewlett Packard Co
 * Copyright (C) 2005 Zou Nan hai <nanhai.zou@intel.com> Intel Corp
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
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <elf.h>
#include <boot/elf_boot.h>
#include <ip_checksum.h>
#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "../../kexec-elf.h"
#include "kexec-ia64.h"
#include "crashdump-ia64.h"
#include <arch/options.h>

static const int probe_debug = 0;
extern unsigned long saved_efi_memmap_size;

/*
 * elf_ia64_probe - sanity check the elf image
 *
 * Make sure that the file image has a reasonable chance of working.
 */
int elf_ia64_probe(const char *buf, off_t len)
{
	struct mem_ehdr ehdr;
	int result;
	result = build_elf_exec_info(buf, len, &ehdr, 0);
	if (result < 0) {
		if (probe_debug) {
			fprintf(stderr, "Not an ELF executable\n");
		}
		return -1;
	}
	/* Verify the architecuture specific bits */
	if (ehdr.e_machine != EM_IA_64) {
		/* for a different architecture */
		if (probe_debug) {
			fprintf(stderr, "Not for this architecture.\n");
		}
		return -1;
	}
	return 0;
}

void elf_ia64_usage(void)
{
	printf("    --command-line=STRING Set the kernel command line to "
			"STRING.\n"
	       "    --append=STRING       Set the kernel command line to "
			"STRING.\n"
	       "    --initrd=FILE         Use FILE as the kernel's initial "
			"ramdisk.\n"
	       "    --noio                Disable I/O in purgatory code.\n"
	       "    --vmm=FILE            Use FILE as the kernel image for a\n"
	       "                          virtual machine monitor "
			"(aka hypervisor)\n");
}

/* Move the crash kerenl physical offset to reserved region
 */
void move_loaded_segments(struct mem_ehdr *ehdr, unsigned long addr)
{
	unsigned i;
	long offset = 0;
	int found = 0;
	struct mem_phdr *phdr;
	for(i = 0; i < ehdr->e_phnum; i++) {
		phdr = &ehdr->e_phdr[i];
		if (phdr->p_type == PT_LOAD) {
			offset = addr - phdr->p_paddr;
			found++;
			break;
		}
	}
	if (!found)
		die("move_loaded_segments: no PT_LOAD region 0x%016x\n", addr);
	ehdr->e_entry += offset;
	for(i = 0; i < ehdr->e_phnum; i++) {
		phdr = &ehdr->e_phdr[i];
		if (phdr->p_type == PT_LOAD)
			phdr->p_paddr += offset;
	}
}

int elf_ia64_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info)
{
	struct mem_ehdr ehdr;
	const char *command_line, *ramdisk=0, *vmm=0, *kernel_buf;
	char *ramdisk_buf = NULL;
	off_t ramdisk_size = 0, kernel_size;
	unsigned long command_line_len;
	unsigned long entry, max_addr, gp_value;
	unsigned long command_line_base, ramdisk_base, image_base;
	unsigned long efi_memmap_base, efi_memmap_size;
	unsigned long boot_param_base;
	unsigned long noio=0;
	int result;
	int opt;
	char *efi_memmap_buf, *boot_param;

	/* See options.h -- add any more there, too. */
	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{"command-line", 1, 0, OPT_APPEND},
		{"append",       1, 0, OPT_APPEND},
		{"initrd",       1, 0, OPT_RAMDISK},
		{"noio",         0, 0, OPT_NOIO},
		{"vmm",          1, 0, OPT_VMM},
		{0, 0, 0, 0},
	};

	static const char short_options[] = KEXEC_ARCH_OPT_STR "";

	command_line = 0;
	while ((opt = getopt_long(argc, argv, short_options,
				  options, 0)) != -1) {
		switch (opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX) {
				break;
			}
		case OPT_APPEND:
			command_line = optarg;
			break;
		case OPT_RAMDISK:
			ramdisk = optarg;
			break;
		case OPT_NOIO:	/* disable PIO and MMIO in purgatory code*/
			noio = 1;
			break;
		case OPT_VMM:
			vmm = optarg;
			break;
		}
	}
	command_line_len = 0;
	if (command_line) {
		command_line_len = strlen(command_line) + 16;
	}

	if (vmm)
		kernel_buf = slurp_decompress_file(vmm, &kernel_size);
	else {
		kernel_buf = buf;
		kernel_size = len;
	}

	/* Parse the Elf file */
	result = build_elf_exec_info(kernel_buf, kernel_size, &ehdr, 0);
	if (result < 0) {
		fprintf(stderr, "ELF parse failed\n");
		free_elf_info(&ehdr);
		return result;
	}

	if (info->kexec_flags & KEXEC_ON_CRASH ) {
		if ((mem_min == 0x00) && (mem_max == ULONG_MAX)) {
			fprintf(stderr, "Failed to find crash kernel region "
				"in %s\n", proc_iomem());
			free_elf_info(&ehdr);
			return -1;
		}
		move_loaded_segments(&ehdr, mem_min);
	} else if (update_loaded_segments(&ehdr) < 0) {
		fprintf(stderr, "Failed to place kernel\n");
		return -1;
	}

	entry = ehdr.e_entry;
	max_addr = elf_max_addr(&ehdr);

	/* Load the Elf data */
	result = elf_exec_load(&ehdr, info);
	if (result < 0) {
		fprintf(stderr, "ELF load failed\n");
		free_elf_info(&ehdr);
		return result;
	}


	/* Load the setup code */
	elf_rel_build_load(info, &info->rhdr, purgatory, purgatory_size,
			0x0, ULONG_MAX, -1, 0);


	if (load_crashdump_segments(info, &ehdr, max_addr, 0,
				&command_line) < 0)
		return -1;

	// reverve 4k for ia64_boot_param
	boot_param = xmalloc(4096);
        boot_param_base = add_buffer(info, boot_param, 4096, 4096, 4096, 0,
                        max_addr, -1);

	elf_rel_set_symbol(&info->rhdr, "__noio",
			   &noio, sizeof(long));

        elf_rel_set_symbol(&info->rhdr, "__boot_param_base",
                        &boot_param_base, sizeof(long));

	// reserve efi_memmap of actual size allocated in production kernel
	efi_memmap_size = saved_efi_memmap_size;
	efi_memmap_buf = xmalloc(efi_memmap_size);
	efi_memmap_base = add_buffer(info, efi_memmap_buf,
			efi_memmap_size, efi_memmap_size, 4096, 0,
			max_addr, -1);

	elf_rel_set_symbol(&info->rhdr, "__efi_memmap_base",
			&efi_memmap_base, sizeof(long));

	elf_rel_set_symbol(&info->rhdr, "__efi_memmap_size",
			&efi_memmap_size, sizeof(long));
	if (command_line) {
		command_line_len = strlen(command_line) + 1;
	}
	if (command_line_len || (info->kexec_flags & KEXEC_ON_CRASH )) {
		char *cmdline = xmalloc(command_line_len);
		strcpy(cmdline, command_line);

		if (info->kexec_flags & KEXEC_ON_CRASH) {
			char buf[128];
			sprintf(buf," max_addr=%lluM min_addr=%lluM",
					mem_max>>20, mem_min>>20);
			command_line_len = strlen(cmdline) + strlen(buf) + 1;
			cmdline = xrealloc(cmdline, command_line_len);
			strcat(cmdline, buf);
		}

		command_line_len = _ALIGN(command_line_len, 16);
		command_line_base = add_buffer(info, cmdline,
				command_line_len, command_line_len,
				getpagesize(), 0UL,
				max_addr, -1);
		elf_rel_set_symbol(&info->rhdr, "__command_line_len",
				&command_line_len, sizeof(long));
		elf_rel_set_symbol(&info->rhdr, "__command_line",
				&command_line_base, sizeof(long));
	}
	
	if (ramdisk) {
		ramdisk_buf = slurp_file(ramdisk, &ramdisk_size);
		ramdisk_base = add_buffer(info, ramdisk_buf, ramdisk_size,
				ramdisk_size,
				getpagesize(), 0, max_addr, -1);
		elf_rel_set_symbol(&info->rhdr, "__ramdisk_base",
				&ramdisk_base, sizeof(long));
		elf_rel_set_symbol(&info->rhdr, "__ramdisk_size",
				&ramdisk_size, sizeof(long));
	}

	if (vmm) {
		image_base = add_buffer(info, buf, len, len,
				getpagesize(), 0, max_addr, -1);
		elf_rel_set_symbol(&info->rhdr, "__vmcode_base",
				&image_base, sizeof(long));
		elf_rel_set_symbol(&info->rhdr, "__vmcode_size",
				&len, sizeof(long));
	}

	gp_value = info->rhdr.rel_addr + 0x200000;
        elf_rel_set_symbol(&info->rhdr, "__gp_value", &gp_value,
                        sizeof(gp_value));

	elf_rel_set_symbol(&info->rhdr, "__kernel_entry", &entry,
			   sizeof(entry));
	free_elf_info(&ehdr);
	return 0;
}
