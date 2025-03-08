/*
 * kexec-elf-rel-cris.c - kexec Elf relocation routines
 * Copyright (C) 2008 AXIS Communications AB
 * Written by Edgar E. Iglesias
 *
 * derived from ../ppc/kexec-elf-rel-ppc.c
 * Copyright (C) 2004 Albert Herranz
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
	if (ehdr->ei_data != ELFDATA2MSB) {
		return 0;
	}
	if (ehdr->ei_class != ELFCLASS32) {
		return 0;
	}
	if (ehdr->e_machine != EM_CRIS) {
		return 0;
	}
	return 1;
}

void machine_apply_elf_rel(struct mem_ehdr *UNUSED(ehdr),
	struct mem_sym *UNUSED(sym), unsigned long r_type, void *location,
	unsigned long address, unsigned long value)
{
	switch(r_type) {

	default:
		die("Unknown rela relocation: %lu\n", r_type);
		break;
	}
	return;
}
