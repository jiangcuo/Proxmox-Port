/*
 * kexec-elf-ppc.c - kexec Elf loader for the PowerPC
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
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <elf.h>
#include <boot/elf_boot.h>
#include <ip_checksum.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"
#include "kexec-ppc.h"
#include <arch/options.h>
#include "../../kexec-syscall.h"
#include "crashdump-powerpc.h"

#include "config.h"
#include "fixup_dtb.h"

static const int probe_debug = 0;

unsigned char reuse_initrd;
int create_flatten_tree(struct kexec_info *, unsigned char **, unsigned long *,
			char *);

#define UPSZ(X) _ALIGN_UP(sizeof(X), 4);
#ifdef WITH_GAMECUBE
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
#endif

int elf_ppc_probe(const char *buf, off_t len)
{

	struct mem_ehdr ehdr;
	int result;
	result = build_elf_exec_info(buf, len, &ehdr, 0);
	if (result < 0) {
		goto out;
	}
	
	/* Verify the architecuture specific bits */
	if (ehdr.e_machine != EM_PPC) {
		/* for a different architecture */
		if (probe_debug) {
			fprintf(stderr, "Not for this architecture.\n");
		}
		result = -1;
		goto out;
	}
	result = 0;
 out:
	free_elf_info(&ehdr);
	return result;
}

#ifdef WITH_GAMECUBE
static void gamecube_hack_addresses(struct mem_ehdr *ehdr)
{
	struct mem_phdr *phdr, *phdr_end;
	phdr_end = ehdr->e_phdr + ehdr->e_phnum;
	for(phdr = ehdr->e_phdr; phdr != phdr_end; phdr++) {
		/*
		 * GameCube ELF kernel is linked with memory mapped
		 * this way (to easily transform it into a DOL
		 * suitable for being loaded with psoload):
		 *
		 * 80000000 - 817fffff 24MB RAM, cached
		 * c0000000 - c17fffff 24MB RAM, not cached
		 *
		 * kexec, instead, needs physical memory layout, so
		 * we clear the upper bits of the address.
		 * (2 bits should be enough, indeed)
		 */
		phdr->p_paddr &= ~0xf0000000;	/* clear bits 0-3, ibm syntax */
	}
}
#endif

/* See options.h -- add any more there, too. */
static const struct option options[] = {
	KEXEC_ARCH_OPTIONS
	{"command-line", 1, 0, OPT_APPEND},
	{"append",       1, 0, OPT_APPEND},
	{"ramdisk",	 1, 0, OPT_RAMDISK},
	{"initrd",	 1, 0, OPT_RAMDISK},
	{"gamecube",     1, 0, OPT_GAMECUBE},
	{"dtb",     1, 0, OPT_DTB},
	{"reuse-node",     1, 0, OPT_NODES},
	{0, 0, 0, 0},
};
static const char short_options[] = KEXEC_ARCH_OPT_STR;

void elf_ppc_usage(void)
{
	printf(
	     "    --command-line=STRING Set the kernel command line to STRING.\n"
	     "    --append=STRING       Set the kernel command line to STRING.\n"
	     "    --ramdisk=<filename>  Initial RAM disk.\n"
	     "    --initrd=<filename>   same as --ramdisk\n"
	     "    --gamecube=1|0        Enable/disable support for ELFs with changed\n"
	     "                          addresses suitable for the GameCube.\n"
	     "    --dtb=<filename>	   Specify device tree blob file.\n"
	     "    --reuse-node=node	   Specify nodes which should be taken from /proc/device-tree.\n"
	     "                          Can be set multiple times.\n"
	     );
}

int elf_ppc_load(int argc, char **argv,	const char *buf, off_t len, 
	struct kexec_info *info)
{
	struct mem_ehdr ehdr;
	char *command_line, *crash_cmdline, *cmdline_buf;
	char *tmp_cmdline;
	int command_line_len, crash_cmdline_len;
	char *dtb;
	int result;
	char *error_msg;
	unsigned long max_addr, hole_addr;
	struct mem_phdr *phdr;
	size_t size;
#ifdef CONFIG_PPC64
	unsigned long toc_addr;
#endif
#ifdef WITH_GAMECUBE
	int target_is_gamecube = 1;
	char *arg_buf;
	size_t arg_bytes;
	unsigned long arg_base;
	struct boot_notes *notes;
	size_t note_bytes;
	unsigned char *setup_start;
	uint32_t setup_size;
#else
	char *seg_buf = NULL;
	off_t seg_size = 0;
	int target_is_gamecube = 0;
	unsigned int addr;
	unsigned long dtb_addr;
	unsigned long dtb_addr_actual;
#endif
	unsigned long kernel_addr;
#define FIXUP_ENTRYS	(20)
	char *fixup_nodes[FIXUP_ENTRYS + 1];
	int cur_fixup = 0;
	int opt;
	char *blob_buf = NULL;
	off_t blob_size = 0;

	command_line = tmp_cmdline = NULL;
	dtb = NULL;
	max_addr = LONG_MAX;
	hole_addr = 0;
	kernel_addr = 0;
	ramdisk = 0;
	result = 0;
	error_msg = NULL;

	while ((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch (opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX) {
				break;
			}
		case OPT_APPEND:
			tmp_cmdline = optarg;
			break;
		case OPT_RAMDISK:
			ramdisk = optarg;
			break;
		case OPT_GAMECUBE:
			target_is_gamecube = atoi(optarg);
			break;

		case OPT_DTB:
			dtb = optarg;
			break;

		case OPT_NODES:
			if (cur_fixup >= FIXUP_ENTRYS) {
				die("The number of entries for the fixup is too large\n");
			}
			fixup_nodes[cur_fixup] = optarg;
			cur_fixup++;
			break;
		}
	}

	if (ramdisk && reuse_initrd)
		die("Can't specify --ramdisk or --initrd with --reuseinitrd\n");

	command_line_len = 0;
	if (tmp_cmdline) {
		command_line = tmp_cmdline;
	} else {
		command_line = get_command_line();
	}
	command_line_len = strlen(command_line);

	fixup_nodes[cur_fixup] = NULL;

	/* Parse the Elf file */
	result = build_elf_exec_info(buf, len, &ehdr, 0);
	if (result < 0) {
		goto out;
	}

#ifdef WITH_GAMECUBE
	if (target_is_gamecube) {
		gamecube_hack_addresses(&ehdr);
	}
#endif

	/* Load the Elf data. Physical load addresses in elf64 header do not
	 * show up correctly. Use user supplied address for now to patch the
	 * elf header
	 */

	phdr = &ehdr.e_phdr[0];
	size = phdr->p_filesz;
	if (size > phdr->p_memsz)
		size = phdr->p_memsz;

	kernel_addr = locate_hole(info, size, 0, 0, max_addr, 1);
#ifdef CONFIG_PPC64
	ehdr.e_phdr[0].p_paddr = (Elf64_Addr)kernel_addr;
#else
	ehdr.e_phdr[0].p_paddr = kernel_addr;
#endif

	/* Load the Elf data */
	result = elf_exec_load(&ehdr, info);
	if (result < 0) {
		goto out;
	}

	/*
	 * Need to append some command line parameters internally in case of
	 * taking crash dumps. Additional segments need to be created.
	 */
	if (info->kexec_flags & KEXEC_ON_CRASH) {
		crash_cmdline = xmalloc(COMMAND_LINE_SIZE);
		memset((void *)crash_cmdline, 0, COMMAND_LINE_SIZE);
		result = load_crashdump_segments(info, crash_cmdline,
						max_addr, 0);
		if (result < 0) {
			result = -1;
			goto out;
		}
		crash_cmdline_len = strlen(crash_cmdline);
	} else {
		crash_cmdline = NULL;
		crash_cmdline_len = 0;
	}

	/*
	 * In case of a toy we take the hardcoded things and an easy setup via
	 * one of the assembly startups. Every thing else should be grown up
	 * and go through the purgatory.
	 */
#ifdef WITH_GAMECUBE
	if (target_is_gamecube) {
		setup_start = setup_dol_start;
		setup_size = setup_dol_size;
		setup_dol_regs.spr8 = ehdr.e_entry;	/* Link Register */
	} else {
		setup_start = setup_simple_start;
		setup_size = setup_simple_size;
		setup_simple_regs.spr8 = ehdr.e_entry;	/* Link Register */
	}
	note_bytes = sizeof(elf_boot_notes) + _ALIGN(command_line_len, 4);
	arg_bytes = note_bytes + _ALIGN(setup_size, 4);

	arg_buf = xmalloc(arg_bytes);
	arg_base = add_buffer(info, 
		arg_buf, arg_bytes, arg_bytes, 4, 0, elf_max_addr(&ehdr), 1);

	notes = (struct boot_notes *)(arg_buf + _ALIGN(setup_size, 4));

	memcpy(arg_buf, setup_start, setup_size);
	memcpy(notes, &elf_boot_notes, sizeof(elf_boot_notes));
	memcpy(notes->command_line, command_line, command_line_len);
	notes->hdr.b_size = note_bytes;
	notes->cmd_hdr.n_descsz = command_line_len;
	notes->hdr.b_checksum = compute_ip_checksum(notes, note_bytes);

	info->entry = (void *)arg_base;
#else
	if (crash_cmdline_len + command_line_len + 1 > COMMAND_LINE_SIZE) {
		printf("Kernel command line exceeds size\n");
		return -1;
	}

	cmdline_buf = xmalloc(COMMAND_LINE_SIZE);
	memset((void *)cmdline_buf, 0, COMMAND_LINE_SIZE);
	if (command_line)
		strncat(cmdline_buf, command_line, command_line_len);
	if (crash_cmdline)
		strncat(cmdline_buf, crash_cmdline,
				sizeof(crash_cmdline) -
				strlen(crash_cmdline) - 1);

	elf_rel_build_load(info, &info->rhdr, (const char *)purgatory,
			purgatory_size, 0, elf_max_addr(&ehdr), 1, 0);

	/* Here we need to initialize the device tree, and find out where
	 * it is going to live so we can place it directly after the
	 * kernel image */
	if (dtb) {
		/* Grab device tree from buffer */
		blob_buf = slurp_file(dtb, &blob_size);
	} else {
		create_flatten_tree(info, (unsigned char **)&blob_buf,
				(unsigned long *)&blob_size, cmdline_buf);
	}
	if (!blob_buf || !blob_size) {
		error_msg = "Device tree seems to be an empty file.\n";
		goto out2;
	}

	/* initial fixup for device tree */
	blob_buf = fixup_dtb_init(info, blob_buf, &blob_size, kernel_addr, &dtb_addr);

	if (ramdisk) {
		seg_buf = slurp_ramdisk_ppc(ramdisk, &seg_size);
		/* load the ramdisk *above* the device tree */
		hole_addr = add_buffer(info, seg_buf, seg_size, seg_size,
				0, dtb_addr + blob_size + 1,  max_addr, -1);
		ramdisk_base = hole_addr;
		ramdisk_size = seg_size;
	}
	if (reuse_initrd) {
		ramdisk_base = initrd_base;
		ramdisk_size = initrd_size;
	}

	if (info->kexec_flags & KEXEC_ON_CRASH && ramdisk_base != 0) {
		if ( (ramdisk_base < crash_base) ||
			(ramdisk_base > crash_base + crash_size) ) {
			printf("WARNING: ramdisk is above crashkernel region!\n");
		}
		else if (ramdisk_base + ramdisk_size > crash_base + crash_size) {
			printf("WARNING: ramdisk overflows crashkernel region!\n");
		}
	}

	/* Perform final fixup on devie tree, i.e. everything beside what
	 * was done above */
	fixup_dtb_finalize(info, blob_buf, &blob_size, fixup_nodes,
			cmdline_buf);
	dtb_addr_actual = add_buffer(info, blob_buf, blob_size, blob_size, 0, dtb_addr,
			kernel_addr + KERNEL_ACCESS_TOP, 1);
	if (dtb_addr_actual != dtb_addr) {
		error_msg = "Error device tree not loadded to address it was expecting to be loaded too!\n";
		goto out2;
	}

	/* 
	 * set various variables for the purgatory.
	 * ehdr.e_entry is a virtual address. we know physical start
	 * address of the kernel (kernel_addr). Find the offset of
	 * e_entry from the virtual start address(e_phdr[0].p_vaddr)
	 * and calculate the actual physical address of the 'kernel entry'.
	 */
	addr = kernel_addr + (ehdr.e_entry - ehdr.e_phdr[0].p_vaddr);
	elf_rel_set_symbol(&info->rhdr, "kernel", &addr, sizeof(addr));

	addr = dtb_addr;
	elf_rel_set_symbol(&info->rhdr, "dt_offset",
					&addr, sizeof(addr));

#define PUL_STACK_SIZE	(16 * 1024)
	addr = locate_hole(info, PUL_STACK_SIZE, 0, 0,
				elf_max_addr(&ehdr), 1);
	addr += PUL_STACK_SIZE;
	elf_rel_set_symbol(&info->rhdr, "stack", &addr, sizeof(addr));
#undef PUL_STACK_SIZE

	/*
	 * Fixup ThreadPointer(r2) for purgatory.
	 * PPC32 ELF ABI expects :
	 * ThreadPointer (TP) = TCB + 0x7000
	 * We manually allocate a TCB space and set the TP
	 * accordingly.
	 */
#define TCB_SIZE 1024
#define TCB_TP_OFFSET 0x7000	/* PPC32 ELF ABI */

	addr = locate_hole(info, TCB_SIZE, 0, 0,
				((unsigned long)elf_max_addr(&ehdr) - TCB_TP_OFFSET),
				1);
	addr += TCB_SIZE + TCB_TP_OFFSET;
	elf_rel_set_symbol(&info->rhdr, "my_thread_ptr", &addr, sizeof(addr));

#undef TCB_SIZE
#undef TCB_TP_OFFSET

	addr = elf_rel_get_addr(&info->rhdr, "purgatory_start");
	info->entry = (void *)addr;

out2:
	free(cmdline_buf);
#endif
out:
	free_elf_info(&ehdr);
	free(crash_cmdline);
	if (!tmp_cmdline)
		free(command_line);
	if (error_msg)
		die("%s", error_msg);

	return result;
}
