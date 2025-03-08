#include <stdio.h>
#include <elf.h>
#include <string.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"
#include "kexec-ppc64.h"

#if defined(_CALL_ELF) && _CALL_ELF == 2
#define STO_PPC64_LOCAL_BIT	5
#define STO_PPC64_LOCAL_MASK	(7 << STO_PPC64_LOCAL_BIT)
#define PPC64_LOCAL_ENTRY_OFFSET(other) \
 (((1 << (((other) & STO_PPC64_LOCAL_MASK) >> STO_PPC64_LOCAL_BIT)) >> 2) << 2)

static unsigned int local_entry_offset(struct mem_sym *sym)
{
	/* If this symbol has a local entry point, use it. */
	return PPC64_LOCAL_ENTRY_OFFSET(sym->st_other);
}
#else
static unsigned int local_entry_offset(struct mem_sym *UNUSED(sym))
{
	return 0;
}
#endif

static struct mem_shdr *toc_section(const struct mem_ehdr *ehdr)
{
	struct mem_shdr *shdr, *shdr_end;
	unsigned char *strtab;

	strtab = (unsigned char *)ehdr->e_shdr[ehdr->e_shstrndx].sh_data;
	shdr_end = &ehdr->e_shdr[ehdr->e_shnum];
	for (shdr = ehdr->e_shdr; shdr != shdr_end; shdr++) {
		if (shdr->sh_size &&
			strcmp((char *)&strtab[shdr->sh_name], ".toc") == 0) {
			return shdr;
		}
	}

	return NULL;
}

int machine_verify_elf_rel(struct mem_ehdr *ehdr)
{
	struct mem_shdr *toc;

	if (ehdr->ei_class != ELFCLASS64) {
		return 0;
	}
	if (ehdr->e_machine != EM_PPC64) {
		return 0;
	}

	/* Ensure .toc is sufficiently aligned.  */
	toc = toc_section(ehdr);
	if (toc && toc->sh_addralign < 256)
		toc->sh_addralign = 256;
	return 1;
}

/* r2 is the TOC pointer: it actually points 0x8000 into the TOC (this
   gives the value maximum span in an instruction which uses a signed
   offset) */
unsigned long my_r2(const struct mem_ehdr *ehdr)
{
	struct mem_shdr *shdr;

	shdr = toc_section(ehdr);
	if (!shdr) {
		die("TOC reloc without a toc section?");
	}

	return shdr->sh_addr + 0x8000;
}

static void do_relative_toc(unsigned long value, uint16_t *location,
	unsigned long mask, int complain_signed)
{
	if (complain_signed && (value + 0x8000 > 0xffff)) {
		die("TOC16 relocation overflows (%lu)\n", value);
	}

	if ((~mask & 0xffff) & value) {
		die("bad TOC16 relocation (%lu)\n", value);
	}

	*location = (*location & ~mask) | (value & mask);
}

void machine_apply_elf_rel(struct mem_ehdr *ehdr, struct mem_sym *sym,
	unsigned long r_type, void *location, unsigned long address,
	unsigned long value)
{
	switch(r_type) {
	case R_PPC64_ADDR32:
		/* Simply set it */
		*(uint32_t *)location = value;
		break;

	case R_PPC64_ADDR64:
	case R_PPC64_REL64:
		/* Simply set it */
		*(uint64_t *)location = value;
		break;

	case R_PPC64_REL32:
		*(uint32_t *)location = value - (uint32_t)location;
		break;

	case R_PPC64_TOC:
		*(uint64_t *)location = my_r2(ehdr);
		break;

	case R_PPC64_TOC16:
		do_relative_toc(value - my_r2(ehdr), location, 0xffff, 1);
		break;

	case R_PPC64_TOC16_DS:
		do_relative_toc(value - my_r2(ehdr), location, 0xfffc, 1);
		break;

	case R_PPC64_TOC16_LO:
		do_relative_toc(value - my_r2(ehdr), location, 0xffff, 0);
		break;

	case R_PPC64_TOC16_LO_DS:
		do_relative_toc(value - my_r2(ehdr), location, 0xfffc, 0);
		break;

	case R_PPC64_TOC16_HI:
		do_relative_toc((value - my_r2(ehdr)) >> 16, location,
			0xffff, 0);
		break;

	case R_PPC64_TOC16_HA:
		do_relative_toc((value - my_r2(ehdr) + 0x8000) >> 16, location,
			0xffff, 0);
		break;

	case R_PPC64_REL24:
		value += local_entry_offset(sym);
		/* Convert value to relative */
		value -= address;
		if (value + 0x2000000 > 0x3ffffff || (value & 3) != 0) {
			die("REL24 %li out of range!\n", (long int)value);
		}

		/* Only replace bits 2 through 26 */
		*(uint32_t *)location = (*(uint32_t *)location & ~0x03fffffc) |
					(value & 0x03fffffc);
		break;

	case R_PPC64_ADDR16_LO:
		*(uint16_t *)location = value & 0xffff;
		break;

	case R_PPC64_ADDR16_HI:
		*(uint16_t *)location = (value >> 16) & 0xffff;
		break;

	case R_PPC64_ADDR16_HA:
		*(uint16_t *)location = (((value + 0x8000) >> 16) & 0xffff);
		break;

	case R_PPC64_ADDR16_HIGHER:
		*(uint16_t *)location = (((uint64_t)value >> 32) & 0xffff);
		break;

	case R_PPC64_ADDR16_HIGHEST:
		*(uint16_t *)location = (((uint64_t)value >> 48) & 0xffff);
		break;

		/* R_PPC64_REL16_HA and R_PPC64_REL16_LO are handled to support
		 * ABIv2 r2 assignment based on r12 for PIC executable.
		 * Here address is know so replace
		 *	0:	addis 2,12,.TOC.-0b@ha
		 *		addi 2,2,.TOC.-0b@l
		 * by
		 *		lis 2,.TOC.@ha
		 *		addi 2,2,.TOC.@l
		 */
	case R_PPC64_REL16_HA:
		/* check that we are dealing with the addis 2,12 instruction */
		if (((*(uint32_t*)location) & 0xffff0000) != 0x3c4c0000)
			die("Unexpected instruction for  R_PPC64_REL16_HA");
		value += my_r2(ehdr);
		/* replacing by lis 2 */
		*(uint32_t *)location = 0x3c400000 + ((value >> 16) & 0xffff);
		break;

	case R_PPC64_REL16_LO:
		/* check that we are dealing with the addi 2,2 instruction */
		if (((*(uint32_t*)location) & 0xffff0000) != 0x38420000)
			die("Unexpected instruction for R_PPC64_REL16_LO");

		value += my_r2(ehdr) - 4;
		*(uint16_t *)location = value & 0xffff;
		break;

	default:
		die("Unknown rela relocation: %lu\n", r_type);
		break;
	}
}
