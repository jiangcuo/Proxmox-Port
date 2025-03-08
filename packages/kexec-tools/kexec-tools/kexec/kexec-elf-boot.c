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

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <elf.h>
#include <boot/elf_boot.h>
#include <ip_checksum.h>
#include <x86/x86-linux.h>
#include "kexec.h"
#include "kexec-elf.h"
#include "kexec-elf-boot.h"


#define UPSZ(X) _ALIGN_UP(sizeof(X), 4)

static struct boot_notes {
	Elf_Bhdr hdr;
	Elf_Nhdr bl_hdr;
	unsigned char bl_desc[UPSZ(BOOTLOADER)];
	Elf_Nhdr blv_hdr;
	unsigned char blv_desc[UPSZ(BOOTLOADER_VERSION)];
	Elf_Nhdr cmd_hdr;
	unsigned char command_line[0];
} boot_notes = {
	.hdr = {
		.b_signature = ELF_BOOT_MAGIC,
		.b_size = sizeof(boot_notes),
		.b_checksum = 0,
		.b_records = 3,
	},
	.bl_hdr = {
		.n_namesz = 0,
		.n_descsz = sizeof(BOOTLOADER),
		.n_type = EBN_BOOTLOADER_NAME,
	},
	.bl_desc = BOOTLOADER,
	.blv_hdr = {
		.n_namesz = 0,
		.n_descsz = sizeof(BOOTLOADER_VERSION),
		.n_type = EBN_BOOTLOADER_VERSION,
	},
	.blv_desc = BOOTLOADER_VERSION,
	.cmd_hdr = {
		.n_namesz = 0,
		.n_descsz = 0,
		.n_type = EBN_COMMAND_LINE,
	},
};

unsigned long elf_boot_notes(
	struct kexec_info *info, unsigned long max_addr,
	const char *cmdline, int cmdline_len)
{
	unsigned long note_bytes;
	unsigned long note_base;
	struct boot_notes *notes;
	note_bytes = sizeof(*notes) + _ALIGN(cmdline_len, 4);
	notes = xmalloc(note_bytes);
	memcpy(notes, &boot_notes, sizeof(boot_notes));
	memcpy(notes->command_line, cmdline, cmdline_len);
	notes->hdr.b_size = note_bytes;
	notes->cmd_hdr.n_descsz = cmdline_len;
	notes->hdr.b_checksum = compute_ip_checksum(notes, note_bytes);

	note_base = add_buffer(info, notes, note_bytes, note_bytes, 
		4, 0, max_addr, 1);

	return note_base;
}
