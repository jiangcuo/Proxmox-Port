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
	if (ehdr->e_machine != EM_ARM) 
	{
		return 0;
	}
	return 1;
}

void machine_apply_elf_rel(struct mem_ehdr *UNUSED(ehdr),
	struct mem_sym *UNUSED(sym), unsigned long r_type, void *location,
	unsigned long address, unsigned long value)
{
	switch(r_type) {
	case R_ARM_ABS32:
		*((uint32_t *)location) += value;
		break;
	case R_ARM_REL32:
		*((uint32_t *)location) += value - address;
		break;
	default:
		die("Unknown rel relocation: %lu\n", r_type);
		break;
	}
}
