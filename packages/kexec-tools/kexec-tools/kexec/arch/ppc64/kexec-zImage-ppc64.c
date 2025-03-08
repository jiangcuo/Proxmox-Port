/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2004  Adam Litke (agl@us.ibm.com)
 * Copyright (C) 2004  IBM Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation (version 2 of the License).
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <linux/elf.h>
#include "../../kexec.h"

#define MAX_HEADERS 32

int zImage_ppc64_probe(FILE *file)
{
	Elf32_Ehdr elf;
	int valid;

	if (fseek(file, 0, SEEK_SET) < 0) {
		fprintf(stderr, "seek error: %s\n",
			strerror(errno));
		return -1;
	}
	if (fread(&elf, sizeof(Elf32_Ehdr), 1, file) != 1) {
		fprintf(stderr, "read error: %s\n",
			strerror(errno));
		return -1;
	}

	if (elf.e_machine == EM_PPC64) {
		fprintf(stderr, "Elf64 not supported\n");
		return -1;
	}

	valid = (elf.e_ident[EI_MAG0]  == ELFMAG0        &&
		elf.e_ident[EI_MAG1]  == ELFMAG1        &&
		elf.e_ident[EI_MAG2]  == ELFMAG2        &&
		elf.e_ident[EI_MAG3]  == ELFMAG3        &&
		elf.e_ident[EI_CLASS] == ELFCLASS32  &&
		elf.e_ident[EI_DATA]  == ELFDATA2MSB &&
		elf.e_type            == ET_EXEC        &&
		elf.e_machine         == EM_PPC);

	return valid ? 0 : -1;
}

int zImage_ppc64_load(FILE *file, int UNUSED(argc), char **UNUSED(argv),
		      void **ret_entry, struct kexec_segment **ret_segments,
		      int *ret_nr_segments)
{
	Elf32_Ehdr elf;
	Elf32_Phdr *p, *ph;
	struct kexec_segment *segment;
	int i;
	unsigned long memsize, filesize, offset, load_loc = 0;

	/* Parse command line arguments */

	/* Read in the Elf32 header */
        if (fseek(file, 0, SEEK_SET) < 0) {
		perror("seek error:");
		return -1;
	}
        if (fread(&elf, sizeof(Elf32_Ehdr), 1, file) != 1) {
		perror("read error: ");
		return -1;
	}
	if (elf.e_phnum > MAX_HEADERS) {
		fprintf(stderr,
			"Only kernels with %i program headers are supported\n",
			MAX_HEADERS);
		return -1;
	}

	/* Read the section header */
	ph = (Elf32_Phdr *)malloc(sizeof(Elf32_Phdr) * elf.e_phnum);
	if (ph == 0) {
		perror("malloc failed: ");
		return -1;
	}
	if (fseek(file, elf.e_phoff, SEEK_SET) < 0) {
		perror("seek failed: ");
		free(ph);
		return -1;
	}
	if (fread(ph, sizeof(Elf32_Phdr) * elf.e_phnum, 1, file) != 1) {
		perror("read error: ");
		free(ph);
		return -1;
	}

	*ret_segments = malloc(elf.e_phnum * sizeof(struct kexec_segment));
	if (*ret_segments == 0) {
		fprintf(stderr, "malloc failed: %s\n",
			strerror(errno));
		free(ph);
		return -1;
	}
	segment = ret_segments[0];

	/* Scan through the program header */
	memsize = filesize = offset = 0;
	p = ph;
	for (i = 0; i < elf.e_phnum; ++i, ++p) {
		if (p->p_type != PT_LOAD || p->p_offset == 0)
			continue;
		if (memsize == 0) {
			offset = p->p_offset;
			memsize = p->p_memsz;
			filesize = p->p_filesz;
			load_loc = p->p_vaddr;
		} else {
			memsize = p->p_offset + p->p_memsz - offset;
			filesize = p->p_offset + p->p_filesz - offset;
		}
	}
	if (memsize == 0) {
		fprintf(stderr, "Can't find a loadable segment.\n");
		free(ph);
		return -1;
	}

	/* Load program segments */
	p = ph;
	segment->buf = malloc(filesize);
	if (segment->buf == 0) {
		perror("malloc failed: ");
		free(ph);
		return -1;
	}
	for (i = 0; i < elf.e_phnum; ++i, ++p) {
		unsigned long mem_offset;
		if (p->p_type != PT_LOAD || p->p_offset == 0)
			continue;

		/* skip to the actual image */
		if (fseek(file, p->p_offset, SEEK_SET) < 0) {
			perror("seek error: ");
			free(ph);
			return -1;
		}
		mem_offset = p->p_vaddr - load_loc;
		if (fread((void *)segment->buf+mem_offset, p->p_filesz, 1,
				file) != 1) {
			perror("read error: ");
			free(ph);
			return -1;
		}
	}
	segment->mem = (void *) load_loc;
	segment->memsz = memsize;
	segment->bufsz = filesize;
	*ret_entry = (void *)(uintptr_t)elf.e_entry;
	*ret_nr_segments = i - 1;
	free(ph);
	return 0;
}

void zImage_ppc64_usage(void)
{
	printf("zImage support is still broken\n");
}
