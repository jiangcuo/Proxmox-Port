#include <stdio.h>
#include <elf.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"

int machine_verify_elf_rel(struct mem_ehdr *ehdr)
{
	if (ehdr->ei_data != ELFDATA2LSB) {
		return 0;
	}
	if (ehdr->ei_class != ELFCLASS64 &&
	    ehdr->ei_class != ELFCLASS32) {  /* x32 */
		return 0;
	}
	if (ehdr->e_machine != EM_X86_64) {
		return 0;
	}
	return 1;
}

static const char *reloc_name(unsigned long r_type)
{
	static const char *r_name[] = {
	"R_X86_64_NONE",
	"R_X86_64_64",
	"R_X86_64_PC32",
	"R_X86_64_GOT32",
	"R_X86_64_PLT32",
	"R_X86_64_COPY",
	"R_X86_64_GLOB_DAT",
	"R_X86_64_JUMP_SLOT",
	"R_X86_64_RELATIVE",
	"R_X86_64_GOTPCREL",
	"R_X86_64_32",
	"R_X86_64_32S",
	"R_X86_64_16",
	"R_X86_64_PC16",
	"R_X86_64_8",
	"R_X86_64_PC8",
	"R_X86_64_DTPMOD64",
	"R_X86_64_DTPOFF64",
	"R_X86_64_TPOFF64",
	"R_X86_64_TLSGD",
	"R_X86_64_TLSLD",
	"R_X86_64_DTPOFF32",
	"R_X86_64_GOTTPOFF",
	"R_X86_64_TPOFF32",
	};
	static char buf[100];
	const char *name;
	if (r_type < (sizeof(r_name)/sizeof(r_name[0]))){
		name = r_name[r_type];
	}
	else {
		sprintf(buf, "R_X86_64_%lu", r_type);
		name = buf;
	}
	return name;
}

void machine_apply_elf_rel(struct mem_ehdr *UNUSED(ehdr),
	struct mem_sym *UNUSED(sym), unsigned long r_type, void *location,
	unsigned long address, unsigned long value)
{
	dbgprintf("%s\n", reloc_name(r_type));
	switch(r_type) {
	case R_X86_64_NONE:
		break;
	case R_X86_64_64:
		*(uint64_t *)location = value;
		break;
	case R_X86_64_32:
		*(uint32_t *)location = value;
		if (value != *(uint32_t *)location)
			goto overflow;
		break;
	case R_X86_64_32S:
		*(uint32_t *)location = value;
		if ((int64_t)value != *(int32_t *)location)
			goto overflow;
		break;
	case R_X86_64_PC32: 
	case R_X86_64_PLT32:
		*(uint32_t *)location = value - address;
		break;
	default:
		die("Unhandled rela relocation: %s\n", reloc_name(r_type));
		break;
	}
	return;
 overflow:
	die("overflow in relocation type %s val %lx\n",
		reloc_name(r_type), value);
}
