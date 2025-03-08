#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "elf.h"
#include <boot/elf_boot.h>
#include "kexec.h"
#include "kexec-elf.h"

static const int probe_debug = 0;

static void load_elf_segments(struct mem_ehdr *ehdr, struct kexec_info *info, unsigned long base)
{
	size_t i;

	/* Read in the PT_LOAD segments */
	for(i = 0; i < ehdr->e_phnum; i++) {
		struct mem_phdr *phdr;
		size_t size;
		phdr = &ehdr->e_phdr[i];
		if (phdr->p_type != PT_LOAD) {
			continue;
		}
		size = phdr->p_filesz;
		if (size > phdr->p_memsz) {
			size = phdr->p_memsz;
		}
		add_segment(info, phdr->p_data, size,
					phdr->p_paddr + base, phdr->p_memsz);
	}
}

static int get_elf_exec_load_base(struct mem_ehdr *ehdr, struct kexec_info *info,
				  unsigned long min, unsigned long max,
				  unsigned long align, unsigned long *base)
{
	unsigned long first, last;
	size_t i;

	/* Note on arm64 and loongarch64:
	 * arm64's vmlinux has virtual address in physical address
	 * field of PT_LOAD segments. So the following validity check
	 * and relocation makes no sense on arm64.
	 * This is also applies to LoongArch.
	 */
	if (ehdr->e_machine == EM_AARCH64 || ehdr->e_machine == EM_LOONGARCH)
		return 0;

	first = ULONG_MAX;
	last  = 0;
	for(i = 0; i < ehdr->e_phnum; i++) {
		unsigned long start, stop;
		struct mem_phdr *phdr;
		phdr = &ehdr->e_phdr[i];
		if ((phdr->p_type != PT_LOAD) ||
			(phdr->p_memsz == 0))
		{
			continue;
		}
		start = phdr->p_paddr;
		stop  = start + phdr->p_memsz;
		if (first > start) {
			first = start;
		}
		if (last < stop) {
			last = stop;
		}
		if (align < phdr->p_align) {
			align = phdr->p_align;
		}
	}

	if ((max - min) < (last - first))
		return -1;

	if (!valid_memory_range(info, min > first ? min : first, max < last ? max : last)) {
		unsigned long hole;
		hole = locate_hole(info, last - first + 1, align, min, max, 1);
		if (hole == ULONG_MAX)
			return -1;

		/* Base is the value that when added
		 * to any virtual address in the file
		 * yields it's load virtual address.
		 */
		*base = hole - first;
	}
	return 0;
}

int build_elf_exec_info(const char *buf, off_t len, struct mem_ehdr *ehdr,
				uint32_t flags)
{
	struct mem_phdr *phdr, *end_phdr;
	int result;
	result = build_elf_info(buf, len, ehdr, flags);
	if (result < 0) {
		return result;
	}
	if ((ehdr->e_type != ET_EXEC) && (ehdr->e_type != ET_DYN) &&
	    (ehdr->e_type != ET_CORE)) {
		/* not an ELF executable */
		if (probe_debug) {
			fprintf(stderr, "Not ELF type ET_EXEC or ET_DYN\n");
		}
		return -1;
	}
	if (!ehdr->e_phdr) {
		/* No program header */
		fprintf(stderr, "No ELF program header\n");
		return -1; 
	}
	end_phdr = &ehdr->e_phdr[ehdr->e_phnum];
	for(phdr = ehdr->e_phdr; phdr != end_phdr; phdr++) {
		/* Kexec does not support loading interpreters.
		 * In addition this check keeps us from attempting
		 * to kexec ordinay executables.
		 */
		if (phdr->p_type == PT_INTERP) {
			fprintf(stderr, "Requires an ELF interpreter\n");
			return -1;
		}
	}

	return 0;
}


int elf_exec_load(struct mem_ehdr *ehdr, struct kexec_info *info)
{
	unsigned long base;
	int result;

	if (!ehdr->e_phdr) {
		fprintf(stderr, "No program header?\n");
		result = -1;
		goto out;
	}

	/* If I have a dynamic executable find it's size
	 * and then find a location for it in memory.
	 */
	base = 0;
	if (ehdr->e_type == ET_DYN) {
		result = get_elf_exec_load_base(ehdr, info, 0, elf_max_addr(ehdr), 0 /* align */, &base);
		if (result < 0)
			goto out;
	}

	load_elf_segments(ehdr, info, base);

	/* Update entry point to reflect new load address*/
	ehdr->e_entry += base;

	result = 0;
 out:
	return result;
}

int elf_exec_load_relocatable(struct mem_ehdr *ehdr, struct kexec_info *info,
			      unsigned long reloc_min, unsigned long reloc_max,
			      unsigned long align)
{
	unsigned long base;
	int result;

	if (reloc_min > reloc_max) {
		fprintf(stderr, "Bad relocation range, start=%lux > end=%lux.\n", reloc_min, reloc_max);
		result = -1;
		goto out;
	}
	if (!ehdr->e_phdr) {
		fprintf(stderr, "No program header?\n");
		result = -1;
		goto out;
	}

	base = 0;
	result = get_elf_exec_load_base(ehdr, info, reloc_min, reloc_max, align, &base);
	if (result < 0)
		goto out;

	load_elf_segments(ehdr, info, base);

	/* Update entry point to reflect new load address*/
	ehdr->e_entry += base;

	result = 0;
 out:
	return result;
}

void elf_exec_build_load(struct kexec_info *info, struct mem_ehdr *ehdr, 
	const char *buf, off_t len, uint32_t flags)
{
	int result;
	/* Parse the Elf file */
	result = build_elf_exec_info(buf, len, ehdr, flags);
	if (result < 0) {
		die("ELF exec parse failed\n");
	}

	/* Load the Elf data */
	result = elf_exec_load(ehdr, info);
	if (result < 0) {
		die("ELF exec load failed\n");
	}
}

void elf_exec_build_load_relocatable(struct kexec_info *info, struct mem_ehdr *ehdr,
				     const char *buf, off_t len, uint32_t flags,
				     unsigned long reloc_min, unsigned long reloc_max,
				     unsigned long align)
{
	int result;
	/* Parse the Elf file */
	result = build_elf_exec_info(buf, len, ehdr, flags);
	if (result < 0) {
		die("%s: ELF exec parse failed\n", __func__);
	}

	/* Load the Elf data */
	result = elf_exec_load_relocatable(ehdr, info, reloc_min, reloc_max, align);
	if (result < 0) {
		die("%s: ELF exec load failed\n", __func__);
	}
}