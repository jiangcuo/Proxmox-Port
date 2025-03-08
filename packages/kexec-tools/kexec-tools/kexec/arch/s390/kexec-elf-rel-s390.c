/*
 * kexec/arch/s390/kexec-elf-rel-s390.c
 *
 * Copyright IBM Corp. 2005,2011
 *
 * Author(s): Heiko Carstens <heiko.carstens@de.ibm.com>
 *
 */

#include <stdio.h>
#include <elf.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"

int machine_verify_elf_rel(struct mem_ehdr *ehdr)
{
	if (ehdr->ei_data != ELFDATA2MSB)
		return 0;
	if (ehdr->ei_class != ELFCLASS64)
		return 0;
	if (ehdr->e_machine != EM_S390)
		return 0;
	return 1;
}

void machine_apply_elf_rel(struct mem_ehdr *UNUSED(ehdr),
			   struct mem_sym *UNUSED(sym),
			   unsigned long r_type,
			   void *loc,
			   unsigned long address,
			   unsigned long val)
{
	switch (r_type) {
	case R_390_8:		/* Direct 8 bit.   */
	case R_390_12:		/* Direct 12 bit.  */
	case R_390_16:		/* Direct 16 bit.  */
	case R_390_20:		/* Direct 20 bit.  */
	case R_390_32:		/* Direct 32 bit.  */
	case R_390_64:		/* Direct 64 bit.  */
		if (r_type == R_390_8)
			*(unsigned char *) loc = val;
		else if (r_type == R_390_12)
			*(unsigned short *) loc = (val & 0xfff) |
				(*(unsigned short *) loc & 0xf000);
		else if (r_type == R_390_16)
			*(unsigned short *) loc = val;
		else if (r_type == R_390_20)
			*(unsigned int *) loc =
				(*(unsigned int *) loc & 0xf00000ff) |
				(val & 0xfff) << 16 | (val & 0xff000) >> 4;
		else if (r_type == R_390_32)
			*(unsigned int *) loc = val;
		else if (r_type == R_390_64)
			*(unsigned long *) loc = val;
		break;
	case R_390_PC16:	/* PC relative 16 bit.  */
	case R_390_PC16DBL:	/* PC relative 16 bit shifted by 1.  */
	case R_390_PC32DBL:	/* PC relative 32 bit shifted by 1.  */
	case R_390_PLT32DBL:	/* 32 bit PC rel. PLT shifted by 1.  */
	case R_390_PC32:	/* PC relative 32 bit.  */
	case R_390_PC64:	/* PC relative 64 bit.	*/
		val -= address;
		if (r_type == R_390_PC16)
			*(unsigned short *) loc = val;
		else if (r_type == R_390_PC16DBL)
			*(unsigned short *) loc = val >> 1;
		else if (r_type == R_390_PC32DBL || r_type == R_390_PLT32DBL)
			*(unsigned int *) loc = val >> 1;
		else if (r_type == R_390_PC32)
			*(unsigned int *) loc = val;
		else if (r_type == R_390_PC64)
			*(unsigned long *) loc = val;
		break;
	default:
		die("Unknown rela relocation: 0x%lx 0x%lx\n", r_type, address);
		break;
	}
}
