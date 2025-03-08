/*
 * kexec-netbsd-sh.c - kexec netbsd loader for the SH
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

static const int probe_debug = 0;
extern const unsigned char netbsd_booter[];

/*
 * netbsd_sh_probe - sanity check the elf image
 *
 * Make sure that the file image has a reasonable chance of working.
 */
int netbsd_sh_probe(const char *buf, off_t UNUSED(len))
{
	Elf32_Ehdr *ehdr;

	ehdr = (Elf32_Ehdr *)buf;
	if(memcmp(buf, ELFMAG, SELFMAG) != 0){
	        return -1;
	}
	if (ehdr->e_machine != EM_SH) {
		return -1;
	}
	return 0;
}

void netbsd_sh_usage(void)
{
	printf(
		" --howto=VALUE        NetBSD kernel boot option.\n"
		" --miniroot=FILE      NetBSD miniroot ramdisk.\n\n");
}

int netbsd_sh_load(int argc, char **argv, const char *buf, off_t UNUSED(len),
	struct kexec_info *info)
{
	const char *howto, *miniroot;
	unsigned long entry, start, size, psz;
	char *miniroot_buf;
	off_t miniroot_length;
	unsigned int howto_value;
	unsigned char *param;
	unsigned long *paraml;
	unsigned char *img;

	int opt;

	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{0, 0, 0, 0},
	};

	static const char short_options[] = KEXEC_ARCH_OPT_STR "";

	howto = miniroot = 0;
	while ((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch (opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX) {
				break;
			}
		case OPT_NBSD_HOWTO:
			howto = optarg;
			break;
		case OPT_NBSD_MROOT:
			miniroot = optarg;
			break;
		}
	}

	/* howto */
	howto_value = 0;
	if(howto){
	        howto_value = strtol(howto, NULL, 0);
	}

	psz = getpagesize();

	/* Parse the Elf file */
	{
	        Elf32_Ehdr *ehdr;
		Elf32_Phdr *phdr;
		unsigned long bbs;
		ehdr = (Elf32_Ehdr *)buf;
		phdr = (Elf32_Phdr *)&buf[ehdr->e_phoff];

		entry     = ehdr->e_entry;
		img       = (unsigned char *)&buf[phdr->p_offset];
		start     = (phdr->p_paddr) & 0x1fffffff;
		bbs       = phdr->p_filesz;
		size      = phdr->p_memsz;

		if(size < bbs){
		        size = bbs;
		}

		size = _ALIGN(size, psz);
		memset(&img[bbs], 0, size-bbs);
		add_segment(info, img, size, start, size);
		start += size;
	}

	/* miniroot file */
	miniroot_buf = 0;
	if (miniroot) {
		miniroot_buf = slurp_file(miniroot, &miniroot_length);
		howto_value |= 0x200;
		size = _ALIGN(miniroot_length, psz);
		add_segment(info, miniroot_buf, size, start, size);
		start += size;
	}

	/* howto & bootinfo */
	param  = xmalloc(4096);
	memset(param, 0, 4096);
	paraml = (unsigned long *) &param[256];
	memcpy(param, netbsd_booter, 256);
	paraml[0] = entry;
	paraml[1] = howto_value;
	add_segment(info, param, 4096, start, 4096);

	/* For now we don't have arguments to pass :( */
	info->entry = (void *) (start | 0xa0000000);
	return 0;
}
