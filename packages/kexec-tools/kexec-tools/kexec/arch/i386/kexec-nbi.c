/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2005  Eric Biederman (ebiederm@xmission.com)
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
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <elf.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"
#include "../../kexec-elf-boot.h"
#include "kexec-x86.h"
#include <arch/options.h>

struct segheader
{
	uint8_t length;
	uint8_t vendortag;
	uint8_t reserved;
	uint8_t flags;
#define NBI_SEG 0x3
#define NBI_SEG_ABSOLUTE  0
#define NBI_SEG_APPEND    1
#define NBI_SEG_NEGATIVE  2
#define NBI_SEG_PREPEND   3
#define NBI_LAST_SEG      (1 << 2)
	uint32_t loadaddr;
	uint32_t imglength;
	uint32_t memlength;
};

struct imgheader
{
#define NBI_MAGIC "\x36\x13\x03\x1b"
	uint8_t magic[4];
#define NBI_RETURNS (1 << 8)
#define NBI_ENTRY32 (1 << 31)
	uint32_t length;	/* and flags */
	struct { uint16_t bx, ds; } segoff;
	union {
		struct { uint16_t ip, cs; } segoff;
		uint32_t linear;
	} execaddr;
};


static const int probe_debug = 0;

int nbi_probe(const char *buf, off_t len)
{
	struct imgheader hdr;
	struct segheader seg;
	off_t seg_off;
	/* If we don't have enough data give up */
	if (((uintmax_t)len < (uintmax_t)sizeof(hdr)) || (len < 512)) {
		return -1;
	}
	memcpy(&hdr, buf, sizeof(hdr));
	if (memcmp(hdr.magic, NBI_MAGIC, sizeof(hdr.magic)) != 0) {
		return -1;
	}
	/* Ensure we have a properly sized header */
	if (((hdr.length & 0xf)*4) != sizeof(hdr)) {
		if (probe_debug) {
			fprintf(stderr, "NBI: Bad vendor header size\n");
		}
		return -1;
	}
	/* Ensure the vendor header is not too large.
	 * This can't actually happen but....
	 */
	if ((((hdr.length & 0xf0) >> 4)*4) > (512 - sizeof(hdr))) {
		if (probe_debug) {
			fprintf(stderr, "NBI: vendor headr too large\n");
		}
		return -1;
	}
	/* Reserved bits are set in the image... */
	if ((hdr.length & 0x7ffffe00)) {
		if (probe_debug) {
			fprintf(stderr, "NBI: Reserved header bits set\n");
		}
		return -1;
	}
	/* If the image can return refuse to load it */
	if (hdr.length & (1 << 8)) {
		if (probe_debug) {
			printf("NBI: image wants to return\n");
		}
		return -1;
	}
	/* Now verify the segments all fit within 512 bytes */
	seg_off = (((hdr.length & 0xf0) >> 4) + (hdr.length & 0x0f)) << 2;
	do {
		memcpy(&seg, buf + seg_off, sizeof(seg));
		if ((seg.length & 0xf) != 4) {
			if (probe_debug) {
				fprintf(stderr, "NBI: Invalid segment length\n");
			}
			return -1;
		}
		seg_off += ((seg.length & 0xf) + ((seg.length >> 4) & 0xf)) << 2;
		if (seg.flags & 0xf8) {
			if (probe_debug) {
				fprintf(stderr, "NBI: segment reserved flags set\n");
			}
			return -1;
		}
		if ((seg.flags & NBI_SEG) == NBI_SEG_NEGATIVE) {
			if (probe_debug) {
				fprintf(stderr, "NBI: negative segment addresses not supported\n");
			}
			return -1;
		}
		if (seg_off > 512) {
			if (probe_debug) {
				fprintf(stderr, "NBI: segment outside 512 header\n");
			}
			return -1;
		}
	} while(!(seg.flags & NBI_LAST_SEG));
	return 0;
}

void nbi_usage(void)
{
	printf(	"\n"
		);
}

int nbi_load(int argc, char **argv, const char *buf, off_t UNUSED(len),
	struct kexec_info *info)
{
	struct imgheader hdr;
	struct segheader seg;
	off_t seg_off;
	off_t file_off;
	uint32_t last0, last1;
	int opt;

	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{ 0, 			0, NULL, 0 },
	};

	static const char short_options[] = KEXEC_OPT_STR "";

	/*
	 * Parse the command line arguments
	 */
	while((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch(opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX) {
				break;
			}
		}
	}
	/* Get a copy of the header */
	memcpy(&hdr, buf, sizeof(hdr));

	/* Load the first 512 bytes */
	add_segment(info, buf + 0, 512, 
		(hdr.segoff.ds << 4) + hdr.segoff.bx, 512);
	
	/* Initialize variables */
	file_off = 512;
	last0 = (hdr.segoff.ds << 4) + hdr.segoff.bx;
	last1 = last0 + 512;

	/* Load the segments  */
	seg_off = (((hdr.length & 0xf0) >> 4) + (hdr.length & 0x0f)) << 2;
	do {
		uint32_t loadaddr;
		memcpy(&seg, buf + seg_off, sizeof(seg));
		seg_off += ((seg.length & 0xf) + ((seg.length >> 4) & 0xf)) << 2;
		if ((seg.flags & NBI_SEG) == NBI_SEG_ABSOLUTE) {
			loadaddr = seg.loadaddr;
		}
		else if ((seg.flags & NBI_SEG) == NBI_SEG_APPEND) {
			loadaddr = last1 + seg.loadaddr;
		}
#if 0
		else if ((seg.flags & NBI_SEG) == NBI_SEG_NEGATIVE) {
			loadaddr = memsize - seg.loadaddr;
		}
#endif
		else if ((seg.flags & NBI_SEG) == NBI_SEG_PREPEND) {
			loadaddr = last0 - seg.loadaddr;
		}
		else {
			printf("warning: unhandled segment of type %0x\n",
				seg.flags & NBI_SEG);
			continue;
		}
		add_segment(info, buf + file_off, seg.imglength,
			loadaddr, seg.memlength);
		last0 = loadaddr;
		last1 = last0 + seg.memlength;
		file_off += seg.imglength;
	} while(!(seg.flags & NBI_LAST_SEG));

	if (hdr.length & NBI_ENTRY32) {
		struct entry32_regs regs32;
		/* Initialize the registers */
		elf_rel_get_symbol(&info->rhdr, "entry32_regs32", &regs32, sizeof(regs32));
		regs32.eip = hdr.execaddr.linear;
		elf_rel_set_symbol(&info->rhdr, "entry32_regs32", &regs32, sizeof(regs32));
	}
	else {
		struct entry32_regs regs32;
		struct entry16_regs regs16;

		/* Initialize the 16 bit registers */
		elf_rel_get_symbol(&info->rhdr, "entry16_regs", &regs16, sizeof(regs16));
		regs16.cs = hdr.execaddr.segoff.cs;
		regs16.ip = hdr.execaddr.segoff.ip;
		elf_rel_set_symbol(&info->rhdr, "entry16_regs", &regs16, sizeof(regs16));

		/* Initialize the 32 bit registers */
		elf_rel_get_symbol(&info->rhdr, "entry32_regs", &regs32, sizeof(regs32));
		regs32.eip = elf_rel_get_addr(&info->rhdr, "entry16");
		elf_rel_set_symbol(&info->rhdr, "entry32_regs", &regs32, sizeof(regs32));
	}
	return 0;
}
