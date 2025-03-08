/*
 * kexec-dol-ppc.c - kexec DOL executable loader for the PowerPC
 * Copyright (C) 2004 Albert Herranz
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
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
#include <elf.h>
#include <boot/elf_boot.h>
#include <ip_checksum.h>
#include "../../kexec.h"
#include "kexec-ppc.h"
#include <arch/options.h>

static int debug = 0;

/*
 * I've found out there DOLs with unaligned and/or overlapping sections.
 * I assume that sizes of sections can be wrong on these DOLs so I trust
 * better start of sections.
 * In order to load DOLs, I first extend sections to page aligned boundaries
 * and then merge overlapping sections starting from lower addresses.
 * -- Albert Herranz
 */

/* DOL related stuff */

#define DOL_HEADER_SIZE    0x100

#define DOL_SECT_MAX_TEXT      7	/* text sections */
#define DOL_SECT_MAX_DATA     11	/* data sections */
#define DOL_MAX_SECT	   (DOL_SECT_MAX_TEXT+DOL_SECT_MAX_DATA)

/* this is the DOL executable header */
typedef struct {
	uint32_t offset_text[DOL_SECT_MAX_TEXT];	/* in the file */
	uint32_t offset_data[DOL_SECT_MAX_DATA];
	uint32_t address_text[DOL_SECT_MAX_TEXT];	/* in memory */
	uint32_t address_data[DOL_SECT_MAX_DATA];
	uint32_t size_text[DOL_SECT_MAX_TEXT];
	uint32_t size_data[DOL_SECT_MAX_DATA];
	uint32_t address_bss;
	uint32_t size_bss;
	uint32_t entry_point;
} dol_header;

#define dol_sect_offset(hptr, index) \
		((index >= DOL_SECT_MAX_TEXT)? \
			hptr->offset_data[index - DOL_SECT_MAX_TEXT] \
			:hptr->offset_text[index])
#define dol_sect_address(hptr, index) \
		((index >= DOL_SECT_MAX_TEXT)? \
			hptr->address_data[index - DOL_SECT_MAX_TEXT] \
			:hptr->address_text[index])
#define dol_sect_size(hptr, index) \
		((index >= DOL_SECT_MAX_TEXT)? \
			hptr->size_data[index - DOL_SECT_MAX_TEXT] \
			:hptr->size_text[index])
#define dol_sect_type(index) \
		((index >= DOL_SECT_MAX_TEXT) ? "data" : "text")

typedef struct {
	uint32_t sects_bitmap;
	uint32_t start;
	uint32_t size;
} dol_segment;

#define dol_seg_end(s1) \
		(s1->start + s1->size)
#define dol_seg_after_sect(s1, s2) \
		(s1->start >= dol_seg_end(s2))
#define dol_seg_overlaps(s1, s2) \
		(!(dol_seg_after_sect(s1,s2) || dol_seg_after_sect(s2,s1)))

/* same as in asm/page.h */
#define PAGE_SHIFT		12
#define PAGE_SIZE		(1UL << PAGE_SHIFT)
#define PAGE_MASK		(~((1 << PAGE_SHIFT) - 1))
#define PAGE_ALIGN(addr)	_ALIGN(addr, PAGE_SIZE)

#define MAX_COMMAND_LINE   256

#define UPSZ(X) _ALIGN_UP(sizeof(X), 4)
static struct boot_notes {
	Elf_Bhdr hdr;
	Elf_Nhdr bl_hdr;
	unsigned char bl_desc[UPSZ(BOOTLOADER)];
	Elf_Nhdr blv_hdr;
	unsigned char blv_desc[UPSZ(BOOTLOADER_VERSION)];
	Elf_Nhdr cmd_hdr;
	unsigned char command_line[0];
} elf_boot_notes = {
        .hdr = {
                .b_signature = 0x0E1FB007,
                .b_size = sizeof(elf_boot_notes),
                .b_checksum = 0,
                .b_records = 3,
        },
        .bl_hdr = {
                .n_namesz = 0,
                .n_descsz = sizeof(BOOTLOADER),
                .n_type = EBN_BOOTLOADER_NAME,
        },
        .bl_desc = BOOTLOADER,
        .blv_hdr = {
                .n_namesz = 0,
                .n_descsz = sizeof(BOOTLOADER_VERSION),
                .n_type = EBN_BOOTLOADER_VERSION,
        },
        .blv_desc = BOOTLOADER_VERSION,
        .cmd_hdr = {
                .n_namesz = 0,
                .n_descsz = 0,
                .n_type = EBN_COMMAND_LINE,
        },
};

void print_sects_bitmap(dol_segment * seg)
{
	int i, first_seen;

	printf("\t" "sects_bitmap");
	first_seen = 0;
	for (i = 0; i < DOL_MAX_SECT; i++) {
		if ((seg->sects_bitmap & (1 << i)) == 0)
			continue;
		printf("%c%d", (first_seen ? ',' : '='), i);
		first_seen = 1;
	}
	printf("\n");
}

void print_dol_segment(dol_segment * seg)
{
	printf("dol segment:\n");
	printf("\t" "start=%08lx, size=%ld (%08lx)\n",
	       (unsigned long)seg->start, (unsigned long)seg->size,
	       (unsigned long)seg->size);
	printf("\t" "end=%08lx\n", (unsigned long)dol_seg_end(seg));
	print_sects_bitmap(seg);
}

int load_dol_segments(dol_segment * seg, int max_segs, dol_header * h)
{
	int i, n, remaining;
	unsigned int start, size;
	unsigned long adj1, adj2, end1;

	n = 0;
	remaining = max_segs;
	for (i = 0; i < DOL_MAX_SECT && remaining > 0; i++) {
		/* zero here means the section is not in use */
		if (dol_sect_size(h, i) == 0)
			continue;

		/* we initially map 1 seg to 1 sect */
		seg->sects_bitmap = (1 << i);

		start = dol_sect_address(h, i);
		size = dol_sect_size(h, i);

		/* page align the segment */
		seg->start = start & PAGE_MASK;
		end1 = start + size;
		adj1 = start - seg->start;
		adj2 = PAGE_ALIGN(end1) - end1;
		seg->size = adj1 + size + adj2;

		//print_dol_segment(seg);

		seg++;
		remaining--;
		n++;
	}
	return n;
}

void fix_dol_segments_overlaps(dol_segment * seg, int max_segs)
{
	int i, j;
	dol_segment *p, *pp;
	long extra_length;

	/* look for overlapping segments and fix them */
	for (i = 0; i < max_segs; i++) {
		p = seg + i;	/* segment p */

		/* not really a segment */
		if (p->size == 0)
			continue;

		/* check if overlaps any previous segments */
		for (j = 0; j < i; j++) {
			pp = seg + j;	/* segment pp */

			/* not a segment or no overlap */
			if (pp->size == 0 || !dol_seg_overlaps(p, pp))
				continue;

			/* merge the two segments */
			if (pp->start < p->start) {
				/* extend pp to include p and delete p */
				extra_length = dol_seg_end(p) - dol_seg_end(pp);
				if (extra_length > 0) {
					pp->size += extra_length;
				}
				pp->sects_bitmap |= p->sects_bitmap;
				p->size = p->start = p->sects_bitmap = 0;

				/* restart the loop because p was deleted */
				i = 0;
				break;
			} else {
				/* extend p to include pp and delete pp */
				extra_length = dol_seg_end(pp) - dol_seg_end(p);
				if (extra_length > 0) {
					p->size += extra_length;
				}
				p->sects_bitmap |= pp->sects_bitmap;
				pp->size = pp->start = pp->sects_bitmap = 0;
			}
		}
	}
}

int dol_ppc_probe(const char *buf, off_t dol_length)
{
	dol_header header, *h;
	int i, valid = 0;

	/* the DOL file should be at least as long as the DOL header */
	if (dol_length < DOL_HEADER_SIZE) {
		if (debug) {
			fprintf(stderr, "Not a DOL file, too short.\n");
		}
		return -1;
	}

	/* read the DOL header */
	memcpy(&header, buf, sizeof(header));
	h = &header;

	/* now perform some sanity checks */
	for (i = 0; i < DOL_MAX_SECT; i++) {
		/* DOL segment MAY NOT be physically stored in the header */
		if ((dol_sect_offset(h, i) != 0)
		    && (dol_sect_offset(h, i) < DOL_HEADER_SIZE)) {
			if (debug) {
				fprintf(stderr,
					"%s segment offset within DOL header\n",
					dol_sect_type(i));
			}
			return -1;
		}

		/* end of physical storage must be within file */
		if ((uintmax_t)(dol_sect_offset(h, i) + dol_sect_size(h, i)) >
		    (uintmax_t)dol_length) {
			if (debug) {
				fprintf(stderr,
					"%s segment past DOL file size\n",
					dol_sect_type(i));
			}
			return -1;
		}

		/* we only should accept DOLs with segments above 2GB */
		if (dol_sect_address(h, i) != 0
		    && !(dol_sect_address(h, i) & 0x80000000)) {
			fprintf(stderr, "warning, %s segment below 2GB\n",
				dol_sect_type(i));
		}

		if (i < DOL_SECT_MAX_TEXT) {
			/* remember that entrypoint was in a code segment */
			if (h->entry_point >= dol_sect_address(h, i)
			    && h->entry_point < dol_sect_address(h, i) +
			    dol_sect_size(h, i))
				valid = 1;
		}
	}

	/* if there is a BSS segment it must^H^H^H^Hshould be above 2GB, too */
	if (h->address_bss != 0 && !(h->address_bss & 0x80000000)) {
		fprintf(stderr, "warning, BSS segment below 2GB\n");
	}

	/* if entrypoint is not within a code segment reject this file */
	if (!valid) {
		if (debug) {
			fprintf(stderr, "Entry point out of text segment\n");
		}
		return -1;
	}

	/* I've got a dol */
	return 0;
}

void dol_ppc_usage(void)
{
	printf
	    ("    --command-line=STRING Set the kernel command line to STRING.\n"
	     "    --append=STRING       Set the kernel command line to STRING.\n");

}

int dol_ppc_load(int argc, char **argv, const char *buf, off_t UNUSED(len),
	struct kexec_info *info)
{
	dol_header header, *h;
	unsigned long entry;
	char *arg_buf;
	size_t arg_bytes;
	unsigned long arg_base;
	struct boot_notes *notes;
	size_t note_bytes;
	const char *command_line;
	int command_line_len;
	unsigned long mstart;
	dol_segment dol_segs[DOL_MAX_SECT];
	unsigned int sects_bitmap;
	unsigned long lowest_start;
	int i, j, k;
	int opt;

	/* See options.h -- add any more there, too. */
        static const struct option options[] = {
                KEXEC_ARCH_OPTIONS
                {"command-line", 1, 0, OPT_APPEND},
                {"append",       1, 0, OPT_APPEND},
                {0, 0, 0, 0},
        };
	static const char short_options[] = KEXEC_ARCH_OPT_STR;

	/*
	 * Parse the command line arguments
	 */
	command_line = 0;
	while ((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch (opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX) {
				break;
			}
		case OPT_APPEND:
			command_line = optarg;
			break;
		}
	}
	command_line_len = 0;
	if (command_line) {
		command_line_len = strlen(command_line) + 1;
	}

	/* read the DOL header */
	memcpy(&header, buf, sizeof(header));
	h = &header;

	/* set entry point */
	entry = h->entry_point;

	/* convert the DOL sections into suitable page aligned segments */
	memset(dol_segs, 0, sizeof(dol_segs));

	load_dol_segments(dol_segs, DOL_MAX_SECT, h);
	fix_dol_segments_overlaps(dol_segs, DOL_MAX_SECT);

	/* load rest of segments */
	for (i = 0; i < DOL_MAX_SECT; i++) {
		unsigned char *seg_buf;
		/* not a segment */
		if (dol_segs[i].size == 0)
			continue;

		//print_dol_segment(&dol_segs[i]);

		/* prepare segment */
		seg_buf = xmalloc(dol_segs[i].size);
		mstart = dol_segs[i].start;
		if (mstart & 0xf0000000) {
			/*
			 * GameCube DOLs expect memory mapped this way:
			 *
			 * 80000000 - 817fffff 24MB RAM, cached
			 * c0000000 - c17fffff 24MB RAM, not cached
			 *
			 * kexec, instead, needs physical memory layout, so
			 * we clear the upper bits of the address.
			 * (2 bits should be enough, indeed)
			 */
			mstart &= ~0xf0000000;	/* clear bits 0-3, ibm syntax */
		}
		add_segment(info,
			seg_buf, dol_segs[i].size, 
			mstart, dol_segs[i].size);
			
			
		/* load sections into segment memory, according to bitmap */
		sects_bitmap = 0;
		while (sects_bitmap != dol_segs[i].sects_bitmap) {
			unsigned char *sec_buf;
			/* find lowest start address for section */
			lowest_start = 0xffffffff;
			for (j = -1, k = 0; k < DOL_MAX_SECT; k++) {
				/* continue if section is already done */
				if ((sects_bitmap & (1 << k)) != 0)
					continue;
				/* do nothing for non sections */
				if ((dol_segs[i].sects_bitmap & (1 << k)) == 0)
					continue;
				/* found new candidate */
				if (dol_sect_address(h, k) < lowest_start) {
					lowest_start = dol_sect_address(h, k);
					j = k;
				}
			}
			/* mark section as being loaded */
			sects_bitmap |= (1 << j);

			/* read it from file to the right place */
			sec_buf = seg_buf + 
			    (dol_sect_address(h, j) - dol_segs[i].start);
			memcpy(sec_buf, buf + dol_sect_offset(h, j), 
				dol_sect_size(h, j));
		}
	}

	/* build the setup glue and argument segment (segment 0) */
	note_bytes = sizeof(elf_boot_notes) + _ALIGN(command_line_len, 4);
	arg_bytes = note_bytes + _ALIGN(setup_dol_size, 4);

	arg_buf = xmalloc(arg_bytes);
	arg_base = add_buffer(info,
		arg_buf, arg_bytes, arg_bytes, 4, 0, 0xFFFFFFFFUL, 1);

	notes = (struct boot_notes *)(arg_buf + _ALIGN(setup_dol_size, 4));

	notes->hdr.b_size = note_bytes;
	notes->cmd_hdr.n_descsz = command_line_len;
	notes->hdr.b_checksum = compute_ip_checksum(notes, note_bytes);

	setup_dol_regs.spr8 = entry;	/* Link Register */

	memcpy(arg_buf, setup_dol_start, setup_dol_size);
	memcpy(notes, &elf_boot_notes, sizeof(elf_boot_notes));
	memcpy(notes->command_line, command_line, command_line_len);

	if (debug) {
		fprintf(stdout, "entry       = %p\n", (void *)arg_base);
		print_segments(stdout, info);
	}

	info->entry = (void *)arg_base;
	return 0;
}
