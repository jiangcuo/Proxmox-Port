/*
 * kexec-elf-rel-m68k.c - kexec Elf relocation routines
 *
 * Copyright (C) 2013 Geert Uytterhoeven
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
	if (ehdr->ei_data != ELFDATA2MSB)
		return 0;
	if (ehdr->ei_class != ELFCLASS32)
		return 0;
	if (ehdr->e_machine != EM_68K)
		return 0;
	return 1;
}

void machine_apply_elf_rel(struct mem_ehdr *UNUSED(ehdr),
			   struct mem_sym *UNUSED(sym),
			   unsigned long r_type,
			   void *UNUSED(location),
			   unsigned long UNUSED(address),
			   unsigned long UNUSED(value))
{
	switch (r_type) {
	default:
		die("Unknown rela relocation: %lu\n", r_type);
		break;
	}
	return;
}
