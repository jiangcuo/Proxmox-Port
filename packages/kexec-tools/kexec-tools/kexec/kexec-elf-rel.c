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

static size_t elf_sym_size(struct mem_ehdr *ehdr)
{
	size_t sym_size = 0;
	if (ehdr->ei_class == ELFCLASS32) {
		sym_size = sizeof(Elf32_Sym);
	}
	else if (ehdr->ei_class == ELFCLASS64) {
		sym_size = sizeof(Elf64_Sym);
	}
	else {
		die("Bad elf class");
	}
	return sym_size;
}

static size_t elf_rel_size(struct mem_ehdr *ehdr)
{
	size_t rel_size = 0;
	if (ehdr->ei_class == ELFCLASS32) {
		rel_size = sizeof(Elf32_Rel);
	}
	else if (ehdr->ei_class == ELFCLASS64) {
		rel_size = sizeof(Elf64_Rel);
	}
	else {
		die("Bad elf class");
	}
	return rel_size;
}

static size_t elf_rela_size(struct mem_ehdr *ehdr)
{
	size_t rel_size = 0;
	if (ehdr->ei_class == ELFCLASS32) {
		rel_size = sizeof(Elf32_Rela);
	}
	else if (ehdr->ei_class == ELFCLASS64) {
		rel_size = sizeof(Elf64_Rela);
	}
	else {
		die("Bad elf class");
	}
	return rel_size;
}

static struct mem_sym elf_sym(struct mem_ehdr *ehdr, const unsigned char *ptr)
{
	struct mem_sym sym = { 0, 0, 0, 0, 0, 0 };
	if (ehdr->ei_class == ELFCLASS32) {
		Elf32_Sym lsym;
		memcpy(&lsym, ptr, sizeof(lsym));
		sym.st_name  = elf32_to_cpu(ehdr, lsym.st_name);
		sym.st_value = elf32_to_cpu(ehdr, lsym.st_value);
		sym.st_size  = elf32_to_cpu(ehdr, lsym.st_size);
		sym.st_info  = lsym.st_info;
		sym.st_other = lsym.st_other;
		sym.st_shndx = elf16_to_cpu(ehdr, lsym.st_shndx);
	}
	else if (ehdr->ei_class == ELFCLASS64) {
		Elf64_Sym lsym;
		memcpy(&lsym, ptr, sizeof(lsym));
		sym.st_name  = elf32_to_cpu(ehdr, lsym.st_name);
		sym.st_value = elf64_to_cpu(ehdr, lsym.st_value);
		sym.st_size  = elf64_to_cpu(ehdr, lsym.st_size);
		sym.st_info  = lsym.st_info;
		sym.st_other = lsym.st_other;
		sym.st_shndx = elf16_to_cpu(ehdr, lsym.st_shndx);
	}
	else {
		die("Bad elf class");
	}
	return sym;
}

static struct mem_rela elf_rel(struct mem_ehdr *ehdr, const unsigned char *ptr)
{
	struct mem_rela rela = { 0, 0, 0, 0 };
	if (ehdr->ei_class == ELFCLASS32) {
		Elf32_Rel lrel;
		memcpy(&lrel, ptr, sizeof(lrel));
		rela.r_offset = elf32_to_cpu(ehdr, lrel.r_offset);
		rela.r_sym    = ELF32_R_SYM(elf32_to_cpu(ehdr, lrel.r_info));
		rela.r_type   = ELF32_R_TYPE(elf32_to_cpu(ehdr, lrel.r_info));
		rela.r_addend = 0;
	}
	else if (ehdr->ei_class == ELFCLASS64) {
		Elf64_Rel lrel;
		memcpy(&lrel, ptr, sizeof(lrel));
		rela.r_offset = elf64_to_cpu(ehdr, lrel.r_offset);
		rela.r_sym    = ELF64_R_SYM(elf64_to_cpu(ehdr, lrel.r_info));
		rela.r_type   = ELF64_R_TYPE(elf64_to_cpu(ehdr, lrel.r_info));
		rela.r_addend = 0;
	}
	else {
		die("Bad elf class");
	}
	return rela;
}

static struct mem_rela elf_rela(struct mem_ehdr *ehdr, const unsigned char *ptr)
{
	struct mem_rela rela = { 0, 0, 0, 0 };
	if (ehdr->ei_class == ELFCLASS32) {
		Elf32_Rela lrela;
		memcpy(&lrela, ptr, sizeof(lrela));
		rela.r_offset = elf32_to_cpu(ehdr, lrela.r_offset);
		rela.r_sym    = ELF32_R_SYM(elf32_to_cpu(ehdr, lrela.r_info));
		rela.r_type   = ELF32_R_TYPE(elf32_to_cpu(ehdr, lrela.r_info));
		rela.r_addend = elf32_to_cpu(ehdr, lrela.r_addend);
	}
	else if (ehdr->ei_class == ELFCLASS64) {
		Elf64_Rela lrela;
		memcpy(&lrela, ptr, sizeof(lrela));
		rela.r_offset = elf64_to_cpu(ehdr, lrela.r_offset);
		rela.r_sym    = ELF64_R_SYM(elf64_to_cpu(ehdr, lrela.r_info));
		rela.r_type   = ELF64_R_TYPE(elf64_to_cpu(ehdr, lrela.r_info));
		rela.r_addend = elf64_to_cpu(ehdr, lrela.r_addend);
	}
	else {
		die("Bad elf class");
	}
	return rela;
}

int build_elf_rel_info(const char *buf, off_t len, struct mem_ehdr *ehdr,
				uint32_t flags)
{
	int result;
	result = build_elf_info(buf, len, ehdr, flags);
	if (result < 0) {
		return result;
	}
	if (ehdr->e_type != ET_REL) {
		/* not an ELF relocate object */
		if (probe_debug) {
			fprintf(stderr, "Not ELF type ET_REL\n");
			fprintf(stderr, "ELF Type: %x\n", ehdr->e_type);
		}
		return -1;
	}
	if (!ehdr->e_shdr) {
		/* No section headers */
		if (probe_debug) {
			fprintf(stderr, "No ELF section headers\n");
		}
		return -1;
	}
	if (!machine_verify_elf_rel(ehdr)) {
		/* It does not meant the native architecture constraints */
		if (probe_debug) {
			fprintf(stderr, "ELF architecture constraint failure\n");
		}
		return -1;
	}
	return 0;
}

static unsigned long get_section_addralign(struct mem_shdr *shdr)
{
	return (shdr->sh_addralign == 0) ? 1 : shdr->sh_addralign;
}

int elf_rel_load(struct mem_ehdr *ehdr, struct kexec_info *info,
	unsigned long min, unsigned long max, int end)
{
	struct mem_shdr *shdr, *shdr_end, *entry_shdr;
	unsigned long entry;
	int result;
	unsigned char *buf;
	unsigned long buf_align, bufsz, bss_align, bsssz, bss_pad;
	unsigned long buf_addr, data_addr, bss_addr;

	if (max > elf_max_addr(ehdr)) {
		max = elf_max_addr(ehdr);
	}
	if (!ehdr->e_shdr) {
		fprintf(stderr, "No section header?\n");
		result = -1;
		goto out;
	}
	shdr_end = &ehdr->e_shdr[ehdr->e_shnum];

	/* Find which section entry is in */
	entry_shdr = NULL;
	entry = ehdr->e_entry;
	for(shdr = ehdr->e_shdr; shdr != shdr_end; shdr++) {
		if (!(shdr->sh_flags & SHF_ALLOC)) {
			continue;
		}
		if (!(shdr->sh_flags & SHF_EXECINSTR)) {
			continue;
		}
		/* Make entry section relative */
		if ((shdr->sh_addr <= ehdr->e_entry) &&
			((shdr->sh_addr + shdr->sh_size) > ehdr->e_entry)) {
			entry_shdr = shdr;
			entry -= shdr->sh_addr;
			break;
		}
	}

	/* Find the memory footprint of the relocatable object */
	buf_align = 1;
	bss_align = 1;
	bufsz = 0;
	bsssz = 0;
	for(shdr = ehdr->e_shdr; shdr != shdr_end; shdr++) {
		if (!(shdr->sh_flags & SHF_ALLOC)) {
			continue;
		}
		if (shdr->sh_type != SHT_NOBITS) {
			unsigned long align;
			align = get_section_addralign(shdr);
			/* See if I need more alignment */
			if (buf_align < align) {
				buf_align = align;
			}
			/* Now align bufsz */
			bufsz = _ALIGN(bufsz, align);
			/* And now add our buffer */
			bufsz += shdr->sh_size;
		}
		else {
			unsigned long align;
			align = get_section_addralign(shdr);
			/* See if I need more alignment */
			if (bss_align < align) {
				bss_align = align;
			}
			/* Now align bsssz */
			bsssz = _ALIGN(bsssz, align);
			/* And now add our buffer */
			bsssz += shdr->sh_size;
		}
	}
	if (buf_align < bss_align) {
		buf_align = bss_align;
	}
	bss_pad = 0;
	if (bufsz & (bss_align - 1)) {
		bss_pad = bss_align - (bufsz & (bss_align - 1));
	}

	/* Allocate where we will put the relocated object */
	buf = xmalloc(bufsz);
	buf_addr = add_buffer(info, buf, bufsz, bufsz + bss_pad + bsssz,
		buf_align, min, max, end);
	ehdr->rel_addr = buf_addr;
	ehdr->rel_size = bufsz + bss_pad + bsssz;

	/* Walk through and find an address for each SHF_ALLOC section */
	data_addr = buf_addr;
	bss_addr  = buf_addr + bufsz + bss_pad;
	for(shdr = ehdr->e_shdr; shdr != shdr_end; shdr++) {
		unsigned long align;
		if (!(shdr->sh_flags & SHF_ALLOC)) {
			continue;
		}
		align = get_section_addralign(shdr);
		if (shdr->sh_type != SHT_NOBITS) {
			unsigned long off;
			/* Adjust the address */
			data_addr = _ALIGN(data_addr, align);

			/* Update the section */
			off = data_addr - buf_addr;
			memcpy(buf + off, shdr->sh_data, shdr->sh_size);
			shdr->sh_addr = data_addr;
			shdr->sh_data = buf + off;

			/* Advance to the next address */
			data_addr += shdr->sh_size;
		} else {
			/* Adjust the address */
			bss_addr = _ALIGN(bss_addr, align);

			/* Update the section */
			shdr->sh_addr = bss_addr;

			/* Advance to the next address */
			bss_addr += shdr->sh_size;
		}
	}
	/* Compute the relocated value for entry, and load it */
	if (entry_shdr) {
		entry += entry_shdr->sh_addr;
		ehdr->e_entry = entry;
	}
	info->entry = (void *)entry;

	/* Now that the load address is known apply relocations */
	for(shdr = ehdr->e_shdr; shdr != shdr_end; shdr++) {
		struct mem_shdr *section, *symtab;
		const unsigned char *strtab;
		size_t rel_size;
		const unsigned char *ptr, *rel_end;
		if ((shdr->sh_type != SHT_RELA) && (shdr->sh_type != SHT_REL)) {
			continue;
		}
		if ((shdr->sh_info > ehdr->e_shnum) ||
			(shdr->sh_link > ehdr->e_shnum))
		{
			die("Invalid section number\n");
		}
		section = &ehdr->e_shdr[shdr->sh_info];
		symtab = &ehdr->e_shdr[shdr->sh_link];

		if (!(section->sh_flags & SHF_ALLOC)) {
			continue;
		}

		if (symtab->sh_link > ehdr->e_shnum) {
			/* Invalid section number? */
			continue;
		}
		strtab = ehdr->e_shdr[symtab->sh_link].sh_data;

		rel_size = 0;
		if (shdr->sh_type == SHT_REL) {
			rel_size = elf_rel_size(ehdr);
		}
		else if (shdr->sh_type == SHT_RELA) {
			rel_size = elf_rela_size(ehdr);
		}
		else {
			die("Cannot find elf rel size\n");
		}
		rel_end = shdr->sh_data + shdr->sh_size;
		for(ptr = shdr->sh_data; ptr < rel_end; ptr += rel_size) {
			struct mem_rela rel = {0};
			struct mem_sym sym;
			const void *location;
			const unsigned char *name;
			unsigned long address, value, sec_base;
			if (shdr->sh_type == SHT_REL) {
				rel = elf_rel(ehdr, ptr);
			}
			else if (shdr->sh_type == SHT_RELA) {
				rel = elf_rela(ehdr, ptr);
			}
			/* the location to change */
			location = section->sh_data + rel.r_offset;

			/* The final address of that location */
			address = section->sh_addr + rel.r_offset;

			/* The relevant symbol */
			sym = elf_sym(ehdr, symtab->sh_data + (rel.r_sym * elf_sym_size(ehdr)));

			if (sym.st_name) {
				name = strtab + sym.st_name;
			}
			else {
				name = ehdr->e_shdr[ehdr->e_shstrndx].sh_data;
				name += ehdr->e_shdr[sym.st_shndx].sh_name;
			}

			dbgprintf("sym: %10s info: %02x other: %02x shndx: %x value: %llx size: %llx\n",
				name,
				sym.st_info,
				sym.st_other,
				sym.st_shndx,
				sym.st_value,
				sym.st_size);

			if (sym.st_shndx == STN_UNDEF) {
			/*
			 * NOTE: ppc64 elf .ro shows up a  UNDEF section.
			 * From Elf 1.2 Spec:
			 * Relocation Entries: If the index is STN_UNDEF,
			 * the undefined symbol index, the relocation uses 0
			 * as the "symbol value".
			 * TOC symbols appear as undefined but should be
			 * resolved as well. Their type is STT_NOTYPE so allow
			 * such symbols to be processed.
			 */
				if (ELF32_ST_TYPE(sym.st_info) != STT_NOTYPE)
					die("Undefined symbol: %s\n", name);
			}
			sec_base = 0;
			if (sym.st_shndx == SHN_COMMON) {
				die("symbol: '%s' in common section\n",
					name);
			}
			else if (sym.st_shndx == SHN_ABS) {
				sec_base = 0;
			}
			else if (sym.st_shndx > ehdr->e_shnum) {
				die("Invalid section: %d for symbol %s\n",
					sym.st_shndx, name);
			}
			else {
				sec_base = ehdr->e_shdr[sym.st_shndx].sh_addr;
			}
			value = sym.st_value;
			value += sec_base;
			value += rel.r_addend;

			dbgprintf("sym: %s value: %lx addr: %lx\n",
				name, value, address);

			machine_apply_elf_rel(ehdr, &sym, rel.r_type,
				(void *)location, address, value);
		}
	}
	result = 0;
 out:
	return result;
}

void elf_rel_build_load(struct kexec_info *info, struct mem_ehdr *ehdr,
	const char *buf, off_t len, unsigned long min, unsigned long max,
	int end, uint32_t flags)
{
	int result;

	/* Parse the Elf file */
	result = build_elf_rel_info(buf, len, ehdr, flags);
	if (result < 0) {
		die("ELF rel parse failed\n");
	}
	/* Load the Elf data */
	result = elf_rel_load(ehdr, info, min, max, end);
	if (result < 0) {
		die("ELF rel load failed\n");
	}
}

int elf_rel_find_symbol(struct mem_ehdr *ehdr,
	const char *name, struct mem_sym *ret_sym)
{
	struct mem_shdr *shdr, *shdr_end;

	if (!ehdr->e_shdr) {
		/* "No section header? */
		return  -1;
	}
	/* Walk through the sections and find the symbol table */
	shdr_end = &ehdr->e_shdr[ehdr->e_shnum];
	for (shdr = ehdr->e_shdr; shdr != shdr_end; shdr++) {
		const char *strtab;
		size_t sym_size;
		const unsigned char *ptr, *sym_end;
		if (shdr->sh_type != SHT_SYMTAB) {
			continue;
		}
		if (shdr->sh_link > ehdr->e_shnum) {
			/* Invalid strtab section number? */
			continue;
		}
		strtab = (char *)ehdr->e_shdr[shdr->sh_link].sh_data;
		/* Walk through the symbol table and find the symbol */
		sym_size = elf_sym_size(ehdr);
		sym_end = shdr->sh_data + shdr->sh_size;
		for(ptr = shdr->sh_data; ptr < sym_end; ptr += sym_size) {
			struct mem_sym sym;
			sym = elf_sym(ehdr, ptr);
			if (ELF32_ST_BIND(sym.st_info) != STB_GLOBAL) {
				continue;
			}
			if (strcmp(strtab + sym.st_name, name) != 0) {
				continue;
			}
			if ((sym.st_shndx == STN_UNDEF) ||
				(sym.st_shndx > ehdr->e_shnum))
			{
				die("Symbol: %s has Bad section index %d\n",
					name, sym.st_shndx);
			}
			*ret_sym = sym;
			return 0;
		}
	}
	/* I did not find it :( */
	return -1;

}

unsigned long elf_rel_get_addr(struct mem_ehdr *ehdr, const char *name)
{
	struct mem_shdr *shdr;
	struct mem_sym sym;
	int result;
	result = elf_rel_find_symbol(ehdr, name, &sym);
	if (result < 0) {
		die("Symbol: %s not found cannot retrive it's address\n",
			name);
	}
	shdr = &ehdr->e_shdr[sym.st_shndx];
	return shdr->sh_addr + sym.st_value;
}

void elf_rel_set_symbol(struct mem_ehdr *ehdr,
	const char *name, const void *buf, size_t size)
{
	unsigned char *sym_buf;
	struct mem_shdr *shdr;
	struct mem_sym sym;
	int result;

	result = elf_rel_find_symbol(ehdr, name, &sym);
	if (result < 0) {
		die("Symbol: %s not found cannot set\n",
			name);
	}
	if (sym.st_size != size) {
		die("Symbol: %s has size: %lld not %zd\n",
			name, sym.st_size, size);
	}
	shdr = &ehdr->e_shdr[sym.st_shndx];
	if (shdr->sh_type == SHT_NOBITS) {
		die("Symbol: %s is in a bss section cannot set\n", name);
	}
	sym_buf = (unsigned char *)(shdr->sh_data + sym.st_value);
	memcpy(sym_buf, buf, size);
}

void elf_rel_get_symbol(struct mem_ehdr *ehdr,
	const char *name, void *buf, size_t size)
{
	const unsigned char *sym_buf;
	struct mem_shdr *shdr;
	struct mem_sym sym;
	int result;

	result = elf_rel_find_symbol(ehdr, name, &sym);
	if (result < 0) {
		die("Symbol: %s not found cannot get\n", name);
	}
	if (sym.st_size != size) {
		die("Symbol: %s has size: %lld not %zd\n",
			name, sym.st_size, size);
	}
	shdr = &ehdr->e_shdr[sym.st_shndx];
	if (shdr->sh_type == SHT_NOBITS) {
		die("Symbol: %s is in a bss section cannot set\n", name);
	}
	sym_buf = shdr->sh_data + sym.st_value;
	memcpy(buf, sym_buf,size);
}
