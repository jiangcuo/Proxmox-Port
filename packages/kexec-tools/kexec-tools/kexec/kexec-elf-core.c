#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include "elf.h"
#include "kexec-elf.h"


int build_elf_core_info(const char *buf, off_t len, struct mem_ehdr *ehdr,
				uint32_t flags)
{
	int result;
	result = build_elf_info(buf, len, ehdr, flags);
	if (result < 0) {
		return result;
	}
	if ((ehdr->e_type != ET_CORE)) {
		/* not an ELF Core */
		fprintf(stderr, "Not ELF type ET_CORE\n");
		return -1;
	}
	if (!ehdr->e_phdr) {
		/* No program header */
		fprintf(stderr, "No ELF program header\n");
		return -1;
	}

	return 0;
}
