/*
 * kexec/arch/s390/kexec-image.c
 *
 * (C) Copyright IBM Corp. 2005
 *
 * Author(s): Rolf Adelsberger <adelsberger@de.ibm.com>
 *            Heiko Carstens <heiko.carstens@de.ibm.com>
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "../../kexec/crashdump.h"
#include "kexec-s390.h"
#include <arch/options.h>
#include <fcntl.h>

static uint64_t crash_base, crash_end;

static void add_segment_check(struct kexec_info *info, const void *buf,
			      size_t bufsz, unsigned long base, size_t memsz)
{
	if (info->kexec_flags & KEXEC_ON_CRASH)
		if (base + memsz > crash_end - crash_base)
			die("Not enough crashkernel memory to load segments\n");
	add_segment(info, buf, bufsz, crash_base + base, memsz);
}

int command_line_add(struct kexec_info *info, const char *str)
{
	char *tmp = NULL;

	tmp = concat_cmdline(info->command_line, str);
	if (!tmp) {
		fprintf(stderr, "out of memory\n");
		return -1;
	}

	free(info->command_line);
	info->command_line = tmp;
	return 0;
}

int image_s390_load_file(int argc, char **argv, struct kexec_info *info)
{
	const char *ramdisk = NULL;
	int opt;

	static const struct option options[] =
		{
			KEXEC_ALL_OPTIONS
			{0,                  0, 0, 0},
		};
	static const char short_options[] = KEXEC_OPT_STR "";

	while ((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch(opt) {
		case OPT_APPEND:
			if (command_line_add(info, optarg))
				return -1;
			break;
		case OPT_RAMDISK:
			ramdisk = optarg;
			break;
		case OPT_REUSE_CMDLINE:
			free(info->command_line);
			info->command_line = get_command_line();
			break;
		}
	}

	if (ramdisk) {
		info->initrd_fd = open(ramdisk, O_RDONLY);
		if (info->initrd_fd == -1) {
			fprintf(stderr, "Could not open initrd file %s:%s\n",
					ramdisk, strerror(errno));
			free(info->command_line);
			info->command_line = NULL;
			return -1;
		}
	}

	if (info->command_line)
		info->command_line_len = strlen(info->command_line) + 1;
	else
		info->command_line_len = 0;
	return 0;
}

int
image_s390_load(int argc, char **argv, const char *kernel_buf,
		off_t kernel_size, struct kexec_info *info)
{
	void *krnl_buffer;
	char *rd_buffer;
	const char *ramdisk;
	off_t ramdisk_len;
	unsigned int ramdisk_origin;
	int opt, ret = -1;

	if (info->file_mode)
		return image_s390_load_file(argc, argv, info);

	static const struct option options[] =
		{
			KEXEC_ALL_OPTIONS
			{0,                  0, 0, 0},
		};
	static const char short_options[] = KEXEC_OPT_STR "";

	ramdisk = NULL;
	ramdisk_len = 0;
	ramdisk_origin = 0;

	while ((opt = getopt_long(argc,argv,short_options,options,0)) != -1) {
		switch(opt) {
		case OPT_APPEND:
			if (command_line_add(info, optarg))
				return -1;
			break;
		case OPT_REUSE_CMDLINE:
			free(info->command_line);
			info->command_line = get_command_line();
			break;
		case OPT_RAMDISK:
			ramdisk = optarg;
			break;
		}
	}

	if (info->kexec_flags & KEXEC_ON_CRASH) {
		if (parse_iomem_single("Crash kernel\n", &crash_base,
				       &crash_end))
			goto out;
	}

	/* Add kernel segment */
	add_segment_check(info, kernel_buf + IMAGE_READ_OFFSET,
		    kernel_size - IMAGE_READ_OFFSET, IMAGE_READ_OFFSET,
		    kernel_size - IMAGE_READ_OFFSET);

	/* We do want to change the kernel image */
	krnl_buffer = (void *) kernel_buf + IMAGE_READ_OFFSET;

	/*
	 * Load ramdisk if present: If image is larger than RAMDISK_ORIGIN_ADDR,
	 * we load the ramdisk directly behind the image with 1 MiB alignment.
	 */
	if (ramdisk) {
		rd_buffer = slurp_file_mmap(ramdisk, &ramdisk_len);
		if (rd_buffer == NULL) {
			fprintf(stderr, "Could not read ramdisk.\n");
			goto out;
		}
		ramdisk_origin = MAX(RAMDISK_ORIGIN_ADDR, kernel_size);
		ramdisk_origin = _ALIGN_UP(ramdisk_origin, 0x100000);
		add_segment_check(info, rd_buffer, ramdisk_len,
				  ramdisk_origin, ramdisk_len);
	}
	if (info->kexec_flags & KEXEC_ON_CRASH) {
		if (load_crashdump_segments(info, crash_base, crash_end))
			goto out;
	} else {
		info->entry = (void *) IMAGE_READ_OFFSET;
	}

	/* Register the ramdisk and crashkernel memory in the kernel. */
	{
		unsigned long long *tmp;

		tmp = krnl_buffer + INITRD_START_OFFS;
		*tmp = (unsigned long long) ramdisk_origin;

		tmp = krnl_buffer + INITRD_SIZE_OFFS;
		*tmp = (unsigned long long) ramdisk_len;

		if (info->kexec_flags & KEXEC_ON_CRASH) {
			tmp = krnl_buffer + OLDMEM_BASE_OFFS;
			*tmp = crash_base;

			tmp = krnl_buffer + OLDMEM_SIZE_OFFS;
			*tmp = crash_end - crash_base + 1;
		}
	}

	if (info->command_line) {
		unsigned long maxsize;
		char *dest = krnl_buffer + COMMAND_LINE_OFFS;

		maxsize = *(unsigned long *)(krnl_buffer + MAX_COMMAND_LINESIZE_OFFS);
		if (!maxsize)
			maxsize = LEGACY_COMMAND_LINESIZE;

		if (strlen(info->command_line) > maxsize-1) {
			fprintf(stderr, "command line too long, maximum allowed size %ld\n",
				maxsize-1);
			goto out;
		}
		strncpy(dest, info->command_line, maxsize-1);
		dest[maxsize-1] = '\0';
	}
	ret = 0;
out:
	free(info->command_line);
	info->command_line = NULL;
	return ret;
}

int 
image_s390_probe(const char *UNUSED(kernel_buf), off_t UNUSED(kernel_size))
{
	/*
	 * Can't reliably tell if an image is valid,
	 * therefore everything is valid.
	 */
	return 0;
}

void
image_s390_usage(void)
{
	printf("--command-line=STRING Set the kernel command line to STRING.\n"
	       "--append=STRING       Set the kernel command line to STRING.\n"
	       "--initrd=FILENAME     Use the file FILENAME as a ramdisk.\n"
	       "--reuse-cmdline       Use kernel command line from running system.\n"
		);
}
