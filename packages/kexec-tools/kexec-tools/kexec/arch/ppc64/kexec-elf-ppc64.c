/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2004  Adam Litke (agl@us.ibm.com)
 * Copyright (C) 2004  IBM Corp.
 * Copyright (C) 2005  R Sharada (sharada@in.ibm.com)
 * Copyright (C) 2006  Mohan Kumar M (mohan@in.ibm.com)
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
#include "../../kexec-elf.h"
#include "../../kexec-syscall.h"
#include "kexec-ppc64.h"
#include "../../fs2dt.h"
#include "crashdump-ppc64.h"
#include <libfdt.h>
#include <arch/fdt.h>
#include <arch/options.h>

uint64_t initrd_base, initrd_size;
unsigned char reuse_initrd = 0;
const char *ramdisk;

int elf_ppc64_probe(const char *buf, off_t len)
{
	struct mem_ehdr ehdr;
	int result;
	result = build_elf_exec_info(buf, len, &ehdr, 0);
	if (result < 0) {
		goto out;
	}

	/* Verify the architecuture specific bits */
	if ((ehdr.e_machine != EM_PPC64) && (ehdr.e_machine != EM_PPC)) {
		/* for a different architecture */
		result = -1;
		goto out;
	}
	result = 0;
 out:
	free_elf_info(&ehdr);
	return result;
}

void arch_reuse_initrd(void)
{
	reuse_initrd = 1;
}

static int read_prop(char *name, void *value, size_t len)
{
	int fd;
	size_t rlen;

	fd = open(name, O_RDONLY);
	if (fd == -1)
		return -1;

	rlen = read(fd, value, len);
	if (rlen < 0)
		fprintf(stderr, "Warning : Can't read %s : %s",
			name, strerror(errno));
	else if (rlen != len)
		fprintf(stderr, "Warning : short read from %s", name);

	close(fd);
	return 0;
}

static int elf_ppc64_load_file(int argc, char **argv, struct kexec_info *info)
{
	int ret = 0;
	char *cmdline, *dtb;
	char *append_cmdline = NULL;
	char *reuse_cmdline = NULL;
	int opt, cmdline_len = 0;

	/* See options.h -- add any more there, too. */
	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{ "command-line",       1, NULL, OPT_APPEND },
		{ "append",             1, NULL, OPT_APPEND },
		{ "ramdisk",            1, NULL, OPT_RAMDISK },
		{ "initrd",             1, NULL, OPT_RAMDISK },
		{ "devicetreeblob",     1, NULL, OPT_DEVICETREEBLOB },
		{ "dtb",                1, NULL, OPT_DEVICETREEBLOB },
		{ "args-linux",		0, NULL, OPT_ARGS_IGNORE },
		{ "reuse-cmdline",	0, NULL, OPT_REUSE_CMDLINE},
		{ 0,                    0, NULL, 0 },
	};

	static const char short_options[] = KEXEC_OPT_STR "";

	/* Parse command line arguments */
	cmdline = 0;
	dtb = 0;
	ramdisk = 0;

	while ((opt = getopt_long(argc, argv, short_options,
					options, 0)) != -1) {
		switch (opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX)
				break;
		case OPT_APPEND:
			append_cmdline = optarg;
			break;
		case OPT_RAMDISK:
			ramdisk = optarg;
			break;
		case OPT_DEVICETREEBLOB:
			dtb = optarg;
			break;
		case OPT_ARGS_IGNORE:
			break;
		case OPT_REUSE_CMDLINE:
			reuse_cmdline = get_command_line();
			break;
		}
	}

	if (dtb)
		die("--dtb not supported while using --kexec-file-syscall.\n");

	if (reuse_initrd)
		die("--reuseinitrd not supported with --kexec-file-syscall.\n");

	cmdline = concat_cmdline(reuse_cmdline, append_cmdline);
	if (!reuse_cmdline)
		free(reuse_cmdline);

	if (cmdline) {
		cmdline_len = strlen(cmdline) + 1;
	} else {
		cmdline = strdup("\0");
		cmdline_len = 1;
	}

	if (ramdisk) {
		info->initrd_fd = open(ramdisk, O_RDONLY);
		if (info->initrd_fd == -1) {
			fprintf(stderr, "Could not open initrd file %s:%s\n",
					ramdisk, strerror(errno));
			ret = -1;
			goto out;
		}
	}

	info->command_line = cmdline;
	info->command_line_len = cmdline_len;
	return ret;
out:
	if (cmdline_len == 1)
		free(cmdline);
	return ret;
}

int elf_ppc64_load(int argc, char **argv, const char *buf, off_t len,
			struct kexec_info *info)
{
	struct mem_ehdr ehdr;
	char *cmdline, *modified_cmdline = NULL;
	char *reuse_cmdline = NULL;
	char *append_cmdline = NULL;
	const char *devicetreeblob;
	uint64_t max_addr, hole_addr;
	char *seg_buf = NULL;
	off_t seg_size = 0;
	struct mem_phdr *phdr;
	size_t size;
#ifdef NEED_RESERVE_DTB
	uint64_t *rsvmap_ptr;
	struct bootblock *bb_ptr;
#endif
	int result, opt;
	uint64_t my_kernel, my_dt_offset;
	uint64_t my_opal_base = 0, my_opal_entry = 0;
	unsigned int my_panic_kernel;
	uint64_t my_stack, my_backup_start;
	uint64_t toc_addr;
	uint32_t my_run_at_load;
	unsigned int slave_code[256/sizeof (unsigned int)], master_entry;

	/* See options.h -- add any more there, too. */
	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{ "command-line",       1, NULL, OPT_APPEND },
		{ "append",             1, NULL, OPT_APPEND },
		{ "ramdisk",            1, NULL, OPT_RAMDISK },
		{ "initrd",             1, NULL, OPT_RAMDISK },
		{ "devicetreeblob",     1, NULL, OPT_DEVICETREEBLOB },
		{ "dtb",                1, NULL, OPT_DEVICETREEBLOB },
		{ "args-linux",		0, NULL, OPT_ARGS_IGNORE },
		{ "reuse-cmdline",	0, NULL, OPT_REUSE_CMDLINE},
		{ 0,                    0, NULL, 0 },
	};

	static const char short_options[] = KEXEC_OPT_STR "";

	if (info->file_mode)
		return elf_ppc64_load_file(argc, argv, info);

	/* Parse command line arguments */
	initrd_base = 0;
	initrd_size = 0;
	cmdline = 0;
	ramdisk = 0;
	devicetreeblob = 0;
	max_addr = 0xFFFFFFFFFFFFFFFFULL;
	hole_addr = 0;

	while ((opt = getopt_long(argc, argv, short_options,
					options, 0)) != -1) {
		switch (opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX)
				break;
		case OPT_APPEND:
			append_cmdline = optarg;
			break;
		case OPT_RAMDISK:
			ramdisk = optarg;
			break;
		case OPT_DEVICETREEBLOB:
			devicetreeblob = optarg;
			break;
		case OPT_ARGS_IGNORE:
			break;
		case OPT_REUSE_CMDLINE:
			reuse_cmdline = get_command_line();
			break;
		}
	}

	cmdline = concat_cmdline(reuse_cmdline, append_cmdline);
	if (!reuse_cmdline)
		free(reuse_cmdline);

	if (!cmdline)
		fprintf(stdout, "Warning: append= option is not passed. Using the first kernel root partition\n");

	if (ramdisk && reuse_initrd)
		die("Can't specify --ramdisk or --initrd with --reuseinitrd\n");

	/* Need to append some command line parameters internally in case of
	 * taking crash dumps.
	 */
	if (info->kexec_flags & KEXEC_ON_CRASH) {
		modified_cmdline = xmalloc(COMMAND_LINE_SIZE);
		memset((void *)modified_cmdline, 0, COMMAND_LINE_SIZE);
		if (cmdline) {
			strncpy(modified_cmdline, cmdline, COMMAND_LINE_SIZE);
			modified_cmdline[COMMAND_LINE_SIZE - 1] = '\0';
		}
	}

	/* Parse the Elf file */
	result = build_elf_exec_info(buf, len, &ehdr, 0);
	if (result < 0) {
		free_elf_info(&ehdr);
		return result;
	}

	/* Load the Elf data. Physical load addresses in elf64 header do not
	 * show up correctly. Use user supplied address for now to patch the
	 * elf header
	 */

	phdr = &ehdr.e_phdr[0];
	size = phdr->p_filesz;
	if (size > phdr->p_memsz)
		size = phdr->p_memsz;

	my_kernel = hole_addr = locate_hole(info, size, 0, 0, max_addr, 1);
	ehdr.e_phdr[0].p_paddr = hole_addr;
	result = elf_exec_load(&ehdr, info);
	if (result < 0) {
		free_elf_info(&ehdr);
		return result;
	}

	/* If panic kernel is being loaded, additional segments need
	 * to be created.
	 */
	if (info->kexec_flags & KEXEC_ON_CRASH) {
		result = load_crashdump_segments(info, modified_cmdline,
						max_addr, 0);
		if (result < 0)
			return -1;
		/* Use new command line. */
		cmdline = modified_cmdline;
	}

	/* Add v2wrap to the current image */
	elf_rel_build_load(info, &info->rhdr, purgatory,
				purgatory_size, 0, max_addr, 1, 0);

	/* Add a ram-disk to the current image
	 * Note: Add the ramdisk after elf_rel_build_load
	 */
	if (ramdisk) {
		if (devicetreeblob) {
			fprintf(stderr,
			"Can't use ramdisk with device tree blob input\n");
			return -1;
		}
		seg_buf = slurp_file(ramdisk, &seg_size);
		hole_addr = add_buffer(info, seg_buf, seg_size, seg_size,
			0, 0, max_addr, 1);
		initrd_base = hole_addr;
		initrd_size = seg_size;
	} /* ramdisk */

	if (devicetreeblob) {
		/* Grab device tree from buffer */
		seg_buf = slurp_file(devicetreeblob, &seg_size);
	} else {
		/* create from fs2dt */
		create_flatten_tree(&seg_buf, &seg_size, cmdline);
	}

	result = fixup_dt(&seg_buf, &seg_size, info->kexec_flags);
	if (result < 0)
		return result;

	my_dt_offset = add_buffer(info, seg_buf, seg_size, seg_size,
				0, 0, max_addr, -1);

#ifdef NEED_RESERVE_DTB
	/* patch reserve map address for flattened device-tree
	 * find last entry (both 0) in the reserve mem list.  Assume DT
	 * entry is before this one
	 */
	bb_ptr = (struct bootblock *)(seg_buf);
	rsvmap_ptr = (uint64_t *)(seg_buf + be32_to_cpu(bb_ptr->off_mem_rsvmap));
	while (*rsvmap_ptr || *(rsvmap_ptr+1))
		rsvmap_ptr += 2;
	rsvmap_ptr -= 2;
	*rsvmap_ptr = cpu_to_be64(my_dt_offset);
	rsvmap_ptr++;
	*rsvmap_ptr = cpu_to_be64((uint64_t)be32_to_cpu(bb_ptr->totalsize));
#endif

	if (read_prop("/proc/device-tree/ibm,opal/opal-base-address",
		      &my_opal_base, sizeof(my_opal_base)) == 0) {
		my_opal_base = be64_to_cpu(my_opal_base);
		elf_rel_set_symbol(&info->rhdr, "opal_base",
				   &my_opal_base, sizeof(my_opal_base));
	}

	if (read_prop("/proc/device-tree/ibm,opal/opal-entry-address",
		      &my_opal_entry, sizeof(my_opal_entry)) == 0) {
		my_opal_entry = be64_to_cpu(my_opal_entry);
		elf_rel_set_symbol(&info->rhdr, "opal_entry",
				   &my_opal_entry, sizeof(my_opal_entry));
	}

	/* Set kernel */
	elf_rel_set_symbol(&info->rhdr, "kernel", &my_kernel, sizeof(my_kernel));

	/* Set dt_offset */
	elf_rel_set_symbol(&info->rhdr, "dt_offset", &my_dt_offset,
				sizeof(my_dt_offset));

	/* get slave code from new kernel, put in purgatory */
	elf_rel_get_symbol(&info->rhdr, "purgatory_start", slave_code,
			sizeof(slave_code));
	master_entry = slave_code[0];
	memcpy(slave_code, phdr->p_data, sizeof(slave_code));
	slave_code[0] = master_entry;
	elf_rel_set_symbol(&info->rhdr, "purgatory_start", slave_code,
				sizeof(slave_code));

	if (info->kexec_flags & KEXEC_ON_CRASH) {
		my_panic_kernel = 1;
		/* Set panic flag */
		elf_rel_set_symbol(&info->rhdr, "panic_kernel",
				&my_panic_kernel, sizeof(my_panic_kernel));

		/* Set backup address */
		my_backup_start = info->backup_start;
		elf_rel_set_symbol(&info->rhdr, "backup_start",
				&my_backup_start, sizeof(my_backup_start));

		/* Tell relocatable kernel to run at load address
		 * via word before slave code in purgatory
		 */

		elf_rel_get_symbol(&info->rhdr, "run_at_load", &my_run_at_load,
				sizeof(my_run_at_load));
		if (my_run_at_load == KERNEL_RUN_AT_ZERO_MAGIC)
			my_run_at_load = 1;
			/* else it should be a fixed offset image */
		elf_rel_set_symbol(&info->rhdr, "run_at_load", &my_run_at_load,
				sizeof(my_run_at_load));
	}

	/* Set stack address */
	my_stack = locate_hole(info, 16*1024, 0, 0, max_addr, 1);
	my_stack += 16*1024;
	elf_rel_set_symbol(&info->rhdr, "stack", &my_stack, sizeof(my_stack));

	/* Set toc */
	toc_addr = my_r2(&info->rhdr);
	elf_rel_set_symbol(&info->rhdr, "my_toc", &toc_addr, sizeof(toc_addr));

	/* Set debug */
	elf_rel_set_symbol(&info->rhdr, "debug", &my_debug, sizeof(my_debug));

	my_kernel = 0;
	my_dt_offset = 0;
	my_panic_kernel = 0;
	my_backup_start = 0;
	my_stack = 0;
	toc_addr = 0;
	my_run_at_load = 0;
	my_debug = 0;
	my_opal_base = 0;
	my_opal_entry = 0;

	elf_rel_get_symbol(&info->rhdr, "opal_base", &my_opal_base,
			   sizeof(my_opal_base));
	elf_rel_get_symbol(&info->rhdr, "opal_entry", &my_opal_entry,
			   sizeof(my_opal_entry));
	elf_rel_get_symbol(&info->rhdr, "kernel", &my_kernel, sizeof(my_kernel));
	elf_rel_get_symbol(&info->rhdr, "dt_offset", &my_dt_offset,
				sizeof(my_dt_offset));
	elf_rel_get_symbol(&info->rhdr, "run_at_load", &my_run_at_load,
				sizeof(my_run_at_load));
	elf_rel_get_symbol(&info->rhdr, "panic_kernel", &my_panic_kernel,
				sizeof(my_panic_kernel));
	elf_rel_get_symbol(&info->rhdr, "backup_start", &my_backup_start,
				sizeof(my_backup_start));
	elf_rel_get_symbol(&info->rhdr, "stack", &my_stack, sizeof(my_stack));
	elf_rel_get_symbol(&info->rhdr, "my_toc", &toc_addr,
				sizeof(toc_addr));
	elf_rel_get_symbol(&info->rhdr, "debug", &my_debug, sizeof(my_debug));

	dbgprintf("info->entry is %p\n", info->entry);
	dbgprintf("kernel is %llx\n", (unsigned long long)my_kernel);
	dbgprintf("dt_offset is %llx\n",
		(unsigned long long)my_dt_offset);
	dbgprintf("run_at_load flag is %x\n", my_run_at_load);
	dbgprintf("panic_kernel is %x\n", my_panic_kernel);
	dbgprintf("backup_start is %llx\n",
		(unsigned long long)my_backup_start);
	dbgprintf("stack is %llx\n", (unsigned long long)my_stack);
	dbgprintf("toc_addr is %llx\n", (unsigned long long)toc_addr);
	dbgprintf("purgatory size is %zu\n", purgatory_size);
	dbgprintf("debug is %d\n", my_debug);
	dbgprintf("opal_base is %llx\n", (unsigned long long) my_opal_base);
	dbgprintf("opal_entry is %llx\n", (unsigned long long) my_opal_entry);

	return 0;
}

void elf_ppc64_usage(void)
{
	printf("     --command-line=<Command line> command line to append.\n");
	printf("     --append=<Command line> same as --command-line.\n");
	printf("     --ramdisk=<filename> Initial RAM disk.\n");
	printf("     --initrd=<filename> same as --ramdisk.\n");
	printf("     --devicetreeblob=<filename> Specify device tree blob file.\n");
	printf("                                 ");
	printf("Not applicable while using --kexec-file-syscall.\n");
	printf("     --reuse-cmdline Use kernel command line from running system.\n");
	printf("     --dtb=<filename> same as --devicetreeblob.\n");

	printf("elf support is still broken\n");
}
