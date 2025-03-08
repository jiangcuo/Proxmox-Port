/*------------------------------------------------------------ -*- C -*-
 *  Eric Biederman <ebiederman@xmission.com>
 *  Erik Arjan Hendriks <hendriks@lanl.gov>
 *
 *  14 December 2004
 *  This file is a derivative of the beoboot image loader, modified
 *  to work with kexec.  
 *
 *  This version is derivative from the orignal mkbootimg.c which is
 *  Copyright (C) 2000 Scyld Computing Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 
 * 
 *--------------------------------------------------------------------*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <x86/x86-linux.h>
#include <boot/beoboot.h>
#include "../../kexec.h"
#include "kexec-x86.h"
#include <arch/options.h>

int beoboot_probe(const char *buf, off_t len)
{
	struct beoboot_header bb_header;
	const char *cmdline, *kernel;
	int result;
	if ((uintmax_t)len < (uintmax_t)sizeof(bb_header)) {
		return -1;
	}
	memcpy(&bb_header, buf, sizeof(bb_header));
	if (memcmp(bb_header.magic, BEOBOOT_MAGIC, 4) != 0) {
		return -1;
	}
	if (bb_header.arch != BEOBOOT_ARCH) {
		return -1;
	}
	/* Make certain a bzImage is packed into there.
	 */
	cmdline = buf + sizeof(bb_header);
	kernel  = cmdline + bb_header.cmdline_size;
	result = bzImage_probe(kernel, bb_header.kernel_size);
	
	return result;
}

void beoboot_usage(void)
{
	printf(	"    --real-mode           Use the kernels real mode entry point.\n"
		);
       
	/* No parameters are parsed */
}

#define SETUP_BASE    0x90000
#define KERN32_BASE  0x100000 /* 1MB */
#define INITRD_BASE 0x1000000 /* 16MB */

int beoboot_load(int argc, char **argv, const char *buf, off_t UNUSED(len),
	struct kexec_info *info)
{
	struct beoboot_header bb_header;
	const char *command_line, *kernel, *initrd;

	int real_mode_entry;
	int opt;
	int result;

	/* See options.h -- add any more there, too. */
	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{ "real-mode",		0, 0, OPT_REAL_MODE },
		{ 0, 			0, 0, 0 },
	};
	static const char short_options[] = KEXEC_ARCH_OPT_STR "";

	/*
	 * Parse the command line arguments
	 */
	real_mode_entry = 0;
	while((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch(opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX) {
				break;
			}
		case OPT_REAL_MODE:
			real_mode_entry = 1;
			break;
		}
	}


	/*
	 * Parse the file
	 */
	memcpy(&bb_header, buf, sizeof(bb_header));
	command_line   = buf + sizeof(bb_header);
	kernel         = command_line + bb_header.cmdline_size;
	initrd         = NULL;
	if (bb_header.flags & BEOBOOT_INITRD_PRESENT) {
		initrd = kernel + bb_header.kernel_size;
	}
	
	result = do_bzImage_load(info,
		kernel,        bb_header.kernel_size,
		command_line,  bb_header.cmdline_size,
		initrd,        bb_header.initrd_size,
		0, 0, real_mode_entry);

	return result;
}

