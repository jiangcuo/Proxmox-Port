/*
 * kexec-elf-rel-sh.c - ELF relocations for SuperH
 * Copyright (C) 2008 Paul Mundt
 *
 * Based on the SHcompact module loader (arch/sh/kernel/module.c) in the
 * Linux kernel, which is written by:
 *
 *	Copyright (C) 2003 - 2008 Kaz Kojima & Paul Mundt
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */
#include <stdio.h>
#include <elf.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"

int machine_verify_elf_rel(struct mem_ehdr *ehdr)
{
	/* Intentionally don't bother with endianness validation, it's
	 * configurable */

	if (ehdr->ei_class != ELFCLASS32)
		return 0;
	if (ehdr->e_machine != EM_SH)
		return 0;

	return 1;
}

void machine_apply_elf_rel(struct mem_ehdr *UNUSED(ehdr),
	struct mem_sym *UNUSED(sym), unsigned long r_type, void *orig_loc,
	unsigned long UNUSED(address), unsigned long relocation)
{
	uint32_t *location = orig_loc;
	uint32_t value;

	switch (r_type) {
	case R_SH_DIR32:
		value = get_unaligned(location);
		value += relocation;
		put_unaligned(value, location);
		break;
	case R_SH_REL32:
		relocation = (relocation - (uint32_t)location);
		value = get_unaligned(location);
		value += relocation;
		put_unaligned(value, location);
		break;
	default:
	        die("Unknown rela relocation: %lu\n", r_type);
		break;
	}
}
