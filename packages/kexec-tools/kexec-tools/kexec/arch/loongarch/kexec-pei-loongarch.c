/*
 * LoongArch kexec PE format binary image support.
 *
 * Copyright (C) 2022 Loongson Technology Corporation Limited.
 *   Youling Tang <tangyouling@loongson.cn>
 *
 * derived from kexec-image-arm64.c
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#define _GNU_SOURCE

#include <limits.h>
#include <errno.h>
#include <elf.h>

#include "kexec.h"
#include "kexec-elf.h"
#include "image-header.h"
#include "kexec-syscall.h"
#include "crashdump-loongarch.h"
#include "kexec-loongarch.h"
#include "arch/options.h"

int pei_loongarch_probe(const char *kernel_buf, off_t kernel_size)
{
	const struct loongarch_image_header *h;

	if (kernel_size < sizeof(struct loongarch_image_header)) {
		dbgprintf("%s: No loongarch image header.\n", __func__);
		return -1;
	}

	h = (const struct loongarch_image_header *)(kernel_buf);

	if (!loongarch_header_check_pe_sig(h)) {
		dbgprintf("%s: Bad loongarch PE image header.\n", __func__);
		return -1;
	}

	return 0;
}

int pei_loongarch_load(int argc, char **argv, const char *buf,
	off_t len, struct kexec_info *info)
{
	int result;
	unsigned long hole_min = 0;
	unsigned long kernel_segment, kernel_entry;
	const struct loongarch_image_header *header;

	header = (const struct loongarch_image_header *)(buf);

	if (loongarch_process_image_header(header))
		return EFAILED;

	kernel_segment = loongarch_locate_kernel_segment(info);

	if (kernel_segment == ULONG_MAX) {
		dbgprintf("%s: Kernel segment is not allocated\n", __func__);
		result = EFAILED;
		goto exit;
	}

	kernel_entry = virt_to_phys(loongarch_header_kernel_entry(header));

	if (info->kexec_flags & KEXEC_ON_CRASH)
		/*
		 * offset addresses in order to load vmlinux.efi into
		 * crash kernel's memory.
		 */
		kernel_entry += crash_reserved_mem[usablemem_rgns.size - 1].start;

	dbgprintf("%s: kernel_segment: %016lx\n", __func__, kernel_segment);
	dbgprintf("%s: kernel_entry:   %016lx\n", __func__, kernel_entry);
	dbgprintf("%s: image_size:     %016lx\n", __func__,
		loongarch_mem.image_size);
	dbgprintf("%s: text_offset:    %016lx\n", __func__,
		loongarch_mem.text_offset);
	dbgprintf("%s: phys_offset:    %016lx\n", __func__,
		loongarch_mem.phys_offset);
	dbgprintf("%s: PE format:      %s\n", __func__,
		(loongarch_header_check_pe_sig(header) ? "yes" : "no"));

	/* Get kernel entry point */
	info->entry = (void *)kernel_entry;

	hole_min = kernel_segment + loongarch_mem.image_size;

	/* Create and initialize elf core header segment */
	if (info->kexec_flags & KEXEC_ON_CRASH) {
		result = load_crashdump_segments(info);
		if (result) {
			dbgprintf("%s: Creating eflcorehdr failed.\n",
								__func__);
			goto exit;
		}
	}

	/* Load the kernel */
	add_segment(info, buf, len, kernel_segment, loongarch_mem.image_size);

	/* Prepare and load dtb and initrd data */
	result = loongarch_load_other_segments(info, hole_min);
	if (result) {
		fprintf(stderr, "kexec: Load dtb and initrd segments failed.\n");
		goto exit;
	}

exit:
	if (result)
		fprintf(stderr, "kexec: load failed.\n");

	return result;
}

void pei_loongarch_usage(void)
{
	printf(
"     An LoongArch PE format binary image, uncompressed, little endian.\n"
"     Typically a vmlinux.efi file.\n\n");
}
