/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2003-2010  Eric Biederman (ebiederm@xmission.com)
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
#include <elf.h>
#include <boot/elf_boot.h>
#include <ip_checksum.h>
#include <x86/x86-linux.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"
#include "../../kexec-syscall.h"
#include "kexec-x86.h"
#include "x86-linux-setup.h"
#include "crashdump-x86.h"
#include <arch/options.h>

static const int probe_debug = 0;
int bzImage_support_efi_boot = 0;

int bzImage_probe(const char *buf, off_t len)
{
	const struct x86_linux_header *header;
	if ((uintmax_t)len < (uintmax_t)(2 * 512)) {
		if (probe_debug) {
			fprintf(stderr, "File is too short to be a bzImage!\n");
		}
		return -1;
	}
	header = (const struct x86_linux_header *)buf;
	if (memcmp(header->header_magic, "HdrS", 4) != 0) {
		if (probe_debug) {
			fprintf(stderr, "Not a bzImage\n");
		}
		return -1;
	}
	if (header->boot_sector_magic != 0xAA55) {
		if (probe_debug) {
			fprintf(stderr, "No x86 boot sector present\n");
		}
		/* No x86 boot sector present */
		return -1;
	}
	if (header->protocol_version < 0x0200) {
		if (probe_debug) {
			fprintf(stderr, "Must be at least protocol version 2.00\n");
		}
		/* Must be at least protocol version 2.00 */
		return -1;
	}
	if ((header->loadflags & 1) == 0) {
		if (probe_debug) {
			fprintf(stderr, "zImage not a bzImage\n");
		}
		/* Not a bzImage */
		return -1;
	}
	/* I've got a bzImage */
	if (probe_debug) {
		fprintf(stderr, "It's a bzImage\n");
	}
	return 0;
}


void bzImage_usage(void)
{
	printf(	"    --real-mode           Use the kernels real mode entry point.\n"
		"    --command-line=STRING Set the kernel command line to STRING.\n"
		"    --append=STRING       Set the kernel command line to STRING.\n"
		"    --reuse-cmdline       Use kernel command line from running system.\n"
		"    --initrd=FILE         Use FILE as the kernel's initial ramdisk.\n"
		"    --ramdisk=FILE        Use FILE as the kernel's initial ramdisk.\n"
		"    --dtb=FILE            Use FILE as devicetree.\n"
		);
       
}

int do_bzImage_load(struct kexec_info *info,
	const char *kernel, off_t kernel_len,
	const char *command_line, off_t command_line_len,
	const char *initrd, off_t initrd_len,
	const char *dtb, off_t dtb_len,
	int real_mode_entry)
{
	struct x86_linux_header setup_header;
	struct x86_linux_param_header *real_mode;
	int setup_sects;
	size_t size;
	int kern16_size;
	unsigned long setup_base, setup_size;
	struct entry32_regs regs32;
	struct entry16_regs regs16;
	unsigned int relocatable_kernel = 0;
	unsigned long kernel32_load_addr;
	char *modified_cmdline;
	unsigned long cmdline_end;
	unsigned long kern16_size_needed;
	unsigned long heap_size = 0;

	/*
	 * Find out about the file I am about to load.
	 */
	if ((uintmax_t)kernel_len < (uintmax_t)(2 * 512)) {
		return -1;
	}
	memcpy(&setup_header, kernel, sizeof(setup_header));
	setup_sects = setup_header.setup_sects;
	if (setup_sects == 0) {
		setup_sects = 4;
	}

	kern16_size = (setup_sects +1) *512;
	if (kernel_len < kern16_size) {
		fprintf(stderr, "BzImage truncated?\n");
		return -1;
	}

	if (setup_header.protocol_version >= 0x0206) {
		if ((uintmax_t)command_line_len >
		    (uintmax_t)setup_header.cmdline_size) {
			dbgprintf("Kernel command line too long for kernel!\n");
			return -1;
		}
	} else {
		if (command_line_len > 255) {
			dbgprintf("WARNING: This kernel may only support 255 byte command lines\n");
		}
	}

	if (setup_header.protocol_version >= 0x0205) {
		relocatable_kernel = setup_header.relocatable_kernel;
		dbgprintf("bzImage is relocatable\n");
	}

	/* Can't use bzImage for crash dump purposes with real mode entry */
	if((info->kexec_flags & KEXEC_ON_CRASH) && real_mode_entry) {
		fprintf(stderr, "Can't use bzImage for crash dump purposes"
				" with real mode entry\n");
		return -1;
	}

	if((info->kexec_flags & KEXEC_ON_CRASH) && !relocatable_kernel) {
		fprintf(stderr, "BzImage is not relocatable. Can't be used"
				" as capture kernel.\n");
		return -1;
	}

	/* Need to append some command line parameters internally in case of
	 * taking crash dumps.
	 */
	if (info->kexec_flags & (KEXEC_ON_CRASH | KEXEC_PRESERVE_CONTEXT)) {
		modified_cmdline = xmalloc(COMMAND_LINE_SIZE);
		memset((void *)modified_cmdline, 0, COMMAND_LINE_SIZE);
		if (command_line) {
			strncpy(modified_cmdline, command_line,
					COMMAND_LINE_SIZE);
			modified_cmdline[COMMAND_LINE_SIZE - 1] = '\0';
		}

		/* If panic kernel is being loaded, additional segments need
		 * to be created. load_crashdump_segments will take care of
		 * loading the segments as high in memory as possible, hence
		 * in turn as away as possible from kernel to avoid being
		 * stomped by the kernel.
		 */
		if (load_crashdump_segments(info, modified_cmdline, -1, 0) < 0)
			return -1;

		/* Use new command line buffer */
		command_line = modified_cmdline;
		command_line_len = strlen(command_line) +1;
	}

	/* Load the trampoline.  This must load at a higher address
	 * than the argument/parameter segment or the kernel will stomp
	 * it's gdt.
	 *
	 * x86_64 purgatory code has got relocations type R_X86_64_32S
	 * that means purgatory got to be loaded within first 2G otherwise
	 * overflow takes place while applying relocations.
	 */
	if (!real_mode_entry && relocatable_kernel)
		elf_rel_build_load(info, &info->rhdr, purgatory, purgatory_size,
					0x3000, 0x7fffffff, -1, 0);
	else
		elf_rel_build_load(info, &info->rhdr, purgatory, purgatory_size,
					0x3000, 640*1024, -1, 0);
	dbgprintf("Loaded purgatory at addr 0x%lx\n", info->rhdr.rel_addr);

	/* The argument/parameter segment */
	if (real_mode_entry) {
		/* need to include size for bss and heap etc */
		if (setup_header.protocol_version >= 0x0201)
			kern16_size_needed = setup_header.heap_end_ptr;
		else
			kern16_size_needed = kern16_size + 8192; /* bss */
		if (kern16_size_needed < kern16_size)
			kern16_size_needed = kern16_size;
		if (kern16_size_needed > 0xfffc)
			die("kern16_size_needed is more then 64k\n");
		heap_size = 0xfffc - kern16_size_needed; /* less 64k */
		heap_size = _ALIGN_DOWN(heap_size, 0x200);
		kern16_size_needed += heap_size;
	} else {
		kern16_size_needed = kern16_size;
		/* need to bigger than size of struct bootparams */
		if (kern16_size_needed < 4096)
			kern16_size_needed = 4096;
	}
	setup_size = kern16_size_needed + command_line_len +
			 PURGATORY_CMDLINE_SIZE;
	real_mode = xmalloc(setup_size);
	memset(real_mode, 0, setup_size);
	if (!real_mode_entry) {
		unsigned long setup_header_size = kernel[0x201] + 0x202 - 0x1f1;

		/* only copy setup_header */
		if (setup_header_size > 0x7f)
			setup_header_size = 0x7f;
		memcpy((unsigned char *)real_mode + 0x1f1, kernel + 0x1f1,
			setup_header_size);
	} else {
		/* copy setup code and setup_header */
		memcpy(real_mode, kernel, kern16_size);
	}

	if (info->kexec_flags & (KEXEC_ON_CRASH | KEXEC_PRESERVE_CONTEXT)) {
		/* If using bzImage for capture kernel, then we will not be
		 * executing real mode code. setup segment can be loaded
		 * anywhere as we will be just reading command line.
		 */
		setup_base = add_buffer(info, real_mode, setup_size, setup_size,
			16, 0x3000, -1, -1);
	}
	else if (real_mode->protocol_version >= 0x0200) {
		/* Careful setup_base must be greater than 8K */
		setup_base = add_buffer(info, real_mode, setup_size, setup_size,
			16, 0x3000, 640*1024, 1);
	} else {
		add_segment(info, real_mode, setup_size, SETUP_BASE, setup_size);
		setup_base = SETUP_BASE;
	}
	dbgprintf("Loaded real-mode code and command line at 0x%lx\n",
			setup_base);

	/* Verify purgatory loads higher than the parameters */
	if (info->rhdr.rel_addr < setup_base) {
		die("Could not put setup code above the kernel parameters\n");
	}
	
	/* The main kernel segment */
	size = kernel_len - kern16_size;

	if (real_mode->protocol_version >=0x0205 && relocatable_kernel) {
		/* Relocatable bzImage */
		unsigned long kern_align = real_mode->kernel_alignment;
		unsigned long kernel32_max_addr = DEFAULT_BZIMAGE_ADDR_MAX;

		if (kernel32_max_addr > real_mode->initrd_addr_max)
			kernel32_max_addr = real_mode->initrd_addr_max;

		kernel32_load_addr = add_buffer(info, kernel + kern16_size,
						size, size, kern_align,
						0x100000, kernel32_max_addr,
						1);
	}
	else {
		kernel32_load_addr = KERN32_BASE;
		add_segment(info, kernel + kern16_size, size,
				kernel32_load_addr, size);
	}
		
	dbgprintf("Loaded 32bit kernel at 0x%lx\n", kernel32_load_addr);

	/* Tell the kernel what is going on */
	setup_linux_bootloader_parameters(info, real_mode, setup_base,
		kern16_size_needed, command_line, command_line_len,
		initrd, initrd_len);

	if (real_mode_entry && real_mode->protocol_version >= 0x0201) {
		real_mode->loader_flags |= 0x80; /* CAN_USE_HEAP */
		real_mode->heap_end_ptr += heap_size - 0x200; /*stack*/
	}

	/* Get the initial register values */
	if (real_mode_entry)
		elf_rel_get_symbol(&info->rhdr, "entry16_regs",
					 &regs16, sizeof(regs16));

	/*
	 * Initialize the 32bit start information.
	 */
	regs32.eax = 0; /* unused */
	regs32.ebx = 0; /* 0 == boot not AP processor start */
	regs32.ecx = 0; /* unused */
	regs32.edx = 0; /* unused */
	regs32.esi = setup_base; /* kernel parameters */
	regs32.edi = 0; /* unused */
	regs32.esp = elf_rel_get_addr(&info->rhdr, "stack_end"); /* stack, unused */
	regs32.ebp = 0; /* unused */
	regs32.eip = kernel32_load_addr; /* kernel entry point */

	/*
	 * Initialize the 16bit start information.
	 */
	if (real_mode_entry) {
		regs16.ds = regs16.es = regs16.fs = regs16.gs = setup_base >> 4;
		regs16.cs = regs16.ds + 0x20;
		regs16.ip = 0;
		/* XXX: Documentation/i386/boot.txt says 'ss' must equal 'ds' */
		regs16.ss = (elf_rel_get_addr(&info->rhdr, "stack_end") - 64*1024) >> 4;
		/* XXX: Documentation/i386/boot.txt says 'sp' must equal heap_end */
		regs16.esp = 0xFFFC;

		printf("Starting the kernel in real mode\n");
		regs32.eip = elf_rel_get_addr(&info->rhdr, "entry16");
		real_mode->kernel_start = kernel32_load_addr;
	}
	if (real_mode_entry && kexec_debug) {
		unsigned long entry16_debug, pre32, first32;
		uint32_t old_first32;
		/* Find the location of the symbols */
		entry16_debug = elf_rel_get_addr(&info->rhdr, "entry16_debug");
		pre32 = elf_rel_get_addr(&info->rhdr, "entry16_debug_pre32");
		first32 = elf_rel_get_addr(&info->rhdr, "entry16_debug_first32");
		
		/* Hook all of the linux kernel hooks */
		real_mode->rmode_switch_cs = entry16_debug >> 4;
		real_mode->rmode_switch_ip = pre32 - entry16_debug;
		old_first32 = real_mode->kernel_start;
		real_mode->kernel_start = first32;
		elf_rel_set_symbol(&info->rhdr, "entry16_debug_old_first32",
			&old_first32, sizeof(old_first32));
	
		regs32.eip = entry16_debug;
	}
	if (real_mode_entry) {
		elf_rel_set_symbol(&info->rhdr, "entry16_regs",
					 &regs16, sizeof(regs16));
		elf_rel_set_symbol(&info->rhdr, "entry16_debug_regs",
					 &regs16, sizeof(regs16));
	}
	elf_rel_set_symbol(&info->rhdr, "entry32_regs", &regs32, sizeof(regs32));
	cmdline_end = setup_base + kern16_size_needed + command_line_len - 1;
	elf_rel_set_symbol(&info->rhdr, "cmdline_end", &cmdline_end,
			   sizeof(unsigned long));

	/* Fill in the information BIOS calls would normally provide. */
	if (!real_mode_entry) {
		setup_linux_system_parameters(info, real_mode);
	}

	if (dtb) {
		setup_linux_dtb(info, real_mode, dtb, dtb_len);
	}

	return 0;
}
	
int bzImage_load(int argc, char **argv, const char *buf, off_t len, 
	struct kexec_info *info)
{
	char *command_line = NULL;
	char *tmp_cmdline = NULL;
	const char *ramdisk, *append = NULL, *dtb;
	char *ramdisk_buf;
	off_t ramdisk_length;
	int command_line_len;
	int real_mode_entry;
	int opt;
	int result;
	char *dtb_buf;
	off_t dtb_length;

	/* See options.h -- add any more there, too. */
	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{ "command-line",	1, 0, OPT_APPEND },
		{ "append",		1, 0, OPT_APPEND },
		{ "reuse-cmdline",	0, 0, OPT_REUSE_CMDLINE },
		{ "initrd",		1, 0, OPT_RAMDISK },
		{ "ramdisk",		1, 0, OPT_RAMDISK },
		{ "real-mode",		0, 0, OPT_REAL_MODE },
		{ "dtb",		1, 0, OPT_DTB },
		{ 0, 			0, 0, 0 },
	};
	static const char short_options[] = KEXEC_ARCH_OPT_STR "d";

	real_mode_entry = 0;
	ramdisk = 0;
	ramdisk_length = 0;
	dtb = 0;
	dtb_length = 0;
	while((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch(opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX) {
				break;
			}
		case OPT_APPEND:
			append = optarg;
			break;
		case OPT_REUSE_CMDLINE:
			tmp_cmdline = get_command_line();
			break;
		case OPT_RAMDISK:
			ramdisk = optarg;
			break;
		case OPT_REAL_MODE:
			real_mode_entry = 1;
			break;
		case OPT_DTB:
			dtb = optarg;
			break;
		}
	}
	command_line = concat_cmdline(tmp_cmdline, append);
	if (tmp_cmdline) {
		free(tmp_cmdline);
	}
	command_line_len = 0;
	if (command_line) {
		command_line_len = strlen(command_line) +1;
	} else {
	    command_line = strdup("\0");
	    command_line_len = 1;
	}
	ramdisk_buf = 0;
	if (ramdisk) {
		ramdisk_buf = slurp_file(ramdisk, &ramdisk_length);
	}
	dtb_buf = 0;
	if (dtb) {
		dtb_buf = slurp_file(dtb, &dtb_length);
	}
	result = do_bzImage_load(info,
		buf, len,
		command_line, command_line_len,
		ramdisk_buf, ramdisk_length,
		dtb_buf, dtb_length,
		real_mode_entry);

	free(command_line);
	return result;
}
