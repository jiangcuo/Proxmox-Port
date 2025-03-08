/*
 * ARM64 kexec elf support.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <linux/elf.h>

#include "arch/options.h"
#include "crashdump-arm64.h"
#include "kexec-arm64.h"
#include "kexec-elf.h"
#include "kexec-syscall.h"

int elf_arm64_probe(const char *kernel_buf, off_t kernel_size)
{
	struct mem_ehdr ehdr;
	int result;

	result = build_elf_exec_info(kernel_buf, kernel_size, &ehdr, 0);

	if (result < 0) {
		dbgprintf("%s: Not an ELF executable.\n", __func__);
		goto on_exit;
	}

	if (ehdr.e_machine != EM_AARCH64) {
		dbgprintf("%s: Not an AARCH64 ELF executable.\n", __func__);
		result = -1;
		goto on_exit;
	}

	result = 0;
on_exit:
	free_elf_info(&ehdr);
	return result;
}

int elf_arm64_load(int argc, char **argv, const char *kernel_buf,
	off_t kernel_size, struct kexec_info *info)
{
	const struct arm64_image_header *header = NULL;
	unsigned long kernel_segment;
	struct mem_ehdr ehdr;
	int result;
	int i;

	if (info->file_mode) {
		fprintf(stderr,
			"ELF executable is not supported in kexec_file\n");

		return EFAILED;
	}

	result = build_elf_exec_info(kernel_buf, kernel_size, &ehdr, 0);

	if (result < 0) {
		dbgprintf("%s: build_elf_exec_info failed\n", __func__);
		goto exit;
	}

	/* Find and process the arm64 image header. */

	for (i = 0; i < ehdr.e_phnum; i++) {
		struct mem_phdr *phdr = &ehdr.e_phdr[i];
		unsigned long header_offset;

		if (phdr->p_type != PT_LOAD)
			continue;

		/*
		 * When CONFIG_ARM64_RANDOMIZE_TEXT_OFFSET=y the image header
		 * could be offset in the elf segment.  The linker script sets
		 * ehdr.e_entry to the start of text.
		 */

		header_offset = ehdr.e_entry - phdr->p_vaddr;

		header = (const struct arm64_image_header *)(
			kernel_buf + phdr->p_offset + header_offset);

		if (!arm64_process_image_header(header)) {
			dbgprintf("%s: e_entry:        %016llx\n", __func__,
				ehdr.e_entry);
			dbgprintf("%s: p_vaddr:        %016llx\n", __func__,
				phdr->p_vaddr);
			dbgprintf("%s: header_offset:  %016lx\n", __func__,
				header_offset);

			break;
		}
	}

	if (i == ehdr.e_phnum) {
		dbgprintf("%s: Valid arm64 header not found\n", __func__);
		result = EFAILED;
		goto exit;
	}

	kernel_segment = arm64_locate_kernel_segment(info);

	if (kernel_segment == ULONG_MAX) {
		dbgprintf("%s: Kernel segment is not allocated\n", __func__);
		result = EFAILED;
		goto exit;
	}

	arm64_mem.vp_offset = _ALIGN_DOWN(ehdr.e_entry, MiB(2));
	if (!(info->kexec_flags & KEXEC_ON_CRASH))
		arm64_mem.vp_offset -= kernel_segment - get_phys_offset();

	dbgprintf("%s: kernel_segment: %016lx\n", __func__, kernel_segment);
	dbgprintf("%s: text_offset:    %016lx\n", __func__,
		arm64_mem.text_offset);
	dbgprintf("%s: image_size:     %016lx\n", __func__,
		arm64_mem.image_size);
	dbgprintf("%s: phys_offset:    %016lx\n", __func__,
		arm64_mem.phys_offset);
	dbgprintf("%s: vp_offset:      %016lx\n", __func__,
		arm64_mem.vp_offset);
	dbgprintf("%s: PE format:      %s\n", __func__,
		(arm64_header_check_pe_sig(header) ? "yes" : "no"));

	/* create and initialize elf core header segment */
	if (info->kexec_flags & KEXEC_ON_CRASH) {
		result = load_crashdump_segments(info);
		if (result) {
			dbgprintf("%s: Creating eflcorehdr failed.\n",
								__func__);
			goto exit;
		}
	}

	/* load the kernel */
	if (info->kexec_flags & KEXEC_ON_CRASH)
		/*
		 * offset addresses in elf header in order to load
		 * vmlinux (elf_exec) into crash kernel's memory
		 */
		fixup_elf_addrs(&ehdr);

	result = elf_exec_load(&ehdr, info);

	if (result) {
		dbgprintf("%s: elf_exec_load failed\n", __func__);
		goto exit;
	}

	/* load additional data */
	result = arm64_load_other_segments(info, kernel_segment
		+ arm64_mem.text_offset);

exit:
	reset_vp_offset();
	free_elf_info(&ehdr);
	if (result)
		fprintf(stderr, "kexec: Bad elf image file, load failed.\n");
	return result;
}

void elf_arm64_usage(void)
{
	printf(
"     An ARM64 ELF image, big or little endian.\n"
"     Typically vmlinux or a stripped version of vmlinux.\n\n");
}
