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
	if (ehdr->e_machine != EM_PPC) {
		return 0;
	}
	return 1;
}

void machine_apply_elf_rel(struct mem_ehdr *UNUSED(ehdr),
	struct mem_sym *UNUSED(sym), unsigned long r_type, void *location,
	unsigned long address, unsigned long value)
{
	switch(r_type) {
	case R_PPC_ADDR32:
		/* Simply set it */
		*(uint32_t *)location = value;
		break;
		
	case R_PPC_ADDR16_LO:
		/* Low half of the symbol */
		*(uint16_t *)location = value;
		break;
		
	case R_PPC_ADDR16_HI:
		*(uint16_t *)location = (value>>16) & 0xffff;
		break;

	case R_PPC_ADDR16_HA:
		/* Sign-adjusted lower 16 bits: PPC ELF ABI says:
		   (((x >> 16) + ((x & 0x8000) ? 1 : 0))) & 0xFFFF.
		   This is the same, only sane.
		*/
		*(uint16_t *)location = (value + 0x8000) >> 16;
		break;
		
	case R_PPC_REL24:
		if ((int)(value - address) < -0x02000000
			|| (int)(value - address) >= 0x02000000)
		{
			die("Symbol more than 16MiB away");
		}
		/* Only replace bits 2 through 26 */
		*(uint32_t *)location
			= (*(uint32_t *)location & ~0x03fffffc)
			| ((value - address)
				& 0x03fffffc);
		break;
		
	case R_PPC_REL32:
		/* 32-bit relative jump. */
		*(uint32_t *)location = value - address;
		break;
	default:
		die("Unknown rela relocation: %lu\n", r_type);
		break;
	}
	return;
}
