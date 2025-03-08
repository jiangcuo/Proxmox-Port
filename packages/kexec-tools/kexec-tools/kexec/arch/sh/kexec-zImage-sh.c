/*
 * kexec-zImage-sh.c - kexec zImage loader for the SH
 * Copyright (C) 2005 kogiidena@eggplant.ddo.jp
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#define _GNU_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
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
#include <arch/options.h>
#include "kexec-sh.h"

static const int probe_debug = 0;

#define HEAD32_KERNEL_START_ADDR 0
#define HEAD32_DECOMPRESS_KERNEL_ADDR 1
#define HEAD32_INIT_STACK_ADDR 2
#define HEAD32_INIT_SR 3
#define HEAD32_INIT_SR_VALUE 0x400000F0

static unsigned long zImage_head32(const char *buf, int offs)
{
	unsigned long *values = (void *)buf;
	int k;

	for (k = (0x200 / 4) - 1; k > 0; k--)
		if (values[k] != 0x00090009) /* not nop + nop padding*/
			return values[k - offs];

	return 0;
}

/*
 * zImage_sh_probe - sanity check the elf image
 *
 * Make sure that the file image has a reasonable chance of working.
 */
int zImage_sh_probe(const char *buf, off_t UNUSED(len))
{
	if (memcmp(&buf[0x202], "HdrS", 4) != 0)
	        return -1;

	if (zImage_head32(buf, HEAD32_INIT_SR) != HEAD32_INIT_SR_VALUE)
	        return -1;

	return 0;
}

void zImage_sh_usage(void)
{
	printf(
    " --append=STRING      Set the kernel command line to STRING.\n"
    " --empty-zero=ADDRESS Set the kernel top ADDRESS. \n\n");

}

int zImage_sh_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info)
{
        char *command_line;
	int opt;
	unsigned long empty_zero, zero_page_base, zero_page_size, k;
	unsigned long image_base;
	char *param;

	static const struct option options[] = {
       	        KEXEC_ARCH_OPTIONS
		{0, 0, 0, 0},
	};

	static const char short_options[] = KEXEC_ARCH_OPT_STR "";

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

	if (!command_line)
	        command_line = get_append();

	/* assume the zero page is the page before the vmlinux entry point.
	 * we don't know the page size though, but 64k seems to be max.
	 * put several 4k zero page copies before the entry point to cover
	 * all combinations.
	 */

	empty_zero = zImage_head32(buf, HEAD32_KERNEL_START_ADDR);

	zero_page_size = 0x10000;
	zero_page_base = virt_to_phys(empty_zero - zero_page_size);

	while (!valid_memory_range(info, zero_page_base,
				   zero_page_base + zero_page_size - 1)) {
		zero_page_base += 0x1000;
		zero_page_size -= 0x1000;
		if (zero_page_size == 0)
			die("Unable to determine zero page size from %p \n",
			    (void *)empty_zero);
	}

	param = xmalloc(zero_page_size);
	for (k = 0; k < (zero_page_size / 0x1000); k++)
		kexec_sh_setup_zero_page(param + (k * 0x1000), 0x1000,
					 command_line);

	add_segment(info, param, zero_page_size,
		    0x80000000 | zero_page_base, zero_page_size);

	/* load image a bit above the zero page, round up to 64k
	 * the zImage will relocate itself, but only up seems supported.
	 */

	image_base = _ALIGN(empty_zero, 0x10000);
	add_segment(info, buf, len, image_base, len);
	info->entry = (void *)virt_to_phys(image_base);
	return 0;
}
