/*
 *  kexec-mb2-x86.c
 *
 *  multiboot2 support for kexec to boot xen.
 *
 *  Copyright (C) 2019 Varad Gautam (vrd at amazon.de), Amazon.com, Inc. or its affiliates.
 *
 *  Parts based on GNU GRUB, Copyright (C) 2000  Free Software Foundation, Inc
 *  Parts taken from kexec-multiboot-x86.c, Eric Biederman (ebiederm@xmission.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
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
#include "kexec-x86.h"
#include "../../kexec-syscall.h"
#include "../../kexec-xen.h"
#include <arch/options.h>

/* From GNU GRUB */
#include <x86/multiboot2.h>
#include <x86/mb_info.h>

/* Framebuffer */
#include <sys/ioctl.h>
#include <linux/fb.h>

extern struct arch_options_t arch_options;

/* Static storage */
static char headerbuf[MULTIBOOT_SEARCH];
static struct multiboot_header *mbh = NULL;
struct multiboot2_header_info {
	struct multiboot_header_tag_information_request *request_tag;
	struct multiboot_header_tag_address *addr_tag;
	struct multiboot_header_tag_entry_address *entry_addr_tag;
	struct multiboot_header_tag_console_flags *console_tag;
	struct multiboot_header_tag_framebuffer *fb_tag;
	struct multiboot_header_tag_module_align *mod_align_tag;
	struct multiboot_header_tag_relocatable *rel_tag;
} mhi;

#define ALIGN_UP(addr, align) \
	((addr + (typeof (addr)) align - 1) & ~((typeof (addr)) align - 1))

int multiboot2_x86_probe(const char *buf, off_t buf_len)
/* Is it a good idea to try booting this file? */
{
	int i, len;

	/* First of all, check that this is an ELF file for either x86 or x86-64 */
	i = elf_x86_any_probe(buf, buf_len, CORE_TYPE_UNDEF);
	if (i < 0)
		return i;

	/* Now look for a multiboot header. */
	len = MULTIBOOT_SEARCH;
	if (len > buf_len)
		len = buf_len;

	memcpy(headerbuf, buf, len);
	if (len < sizeof(struct multiboot_header)) {
		/* Short file */
		return -1;
	}
	for (mbh = (struct multiboot_header *) headerbuf;
	     ((char *) mbh <= (char *) headerbuf + len - sizeof(struct multiboot_header));
	     mbh = (struct multiboot_header *) ((char *) mbh + MULTIBOOT_HEADER_ALIGN)) {
		if (mbh->magic == MULTIBOOT2_HEADER_MAGIC
		    && !((mbh->magic+mbh->architecture+mbh->header_length+mbh->checksum) & 0xffffffff)) {
			/* Found multiboot header. */
			return 0;
		}
	}
	/* Not multiboot */
	return -1;
}

void multiboot2_x86_usage(void)
/* Multiboot-specific options */
{
	printf("    --command-line=STRING        Set the kernel command line to STRING.\n");
	printf("    --reuse-cmdline       	 Use kernel command line from running system.\n");
	printf("    --module=\"MOD arg1 arg2...\"  Load module MOD with command-line \"arg1...\"\n");
	printf("                                 (can be used multiple times).\n");
}

static size_t
multiboot2_get_mbi_size(int ranges, int cmdline_size, int modcount, int modcmd_size)
{
	size_t mbi_size;

	mbi_size = (2 * sizeof (uint32_t) /* u32 total_size, u32 reserved */
		+ ALIGN_UP (sizeof (struct multiboot_tag_basic_meminfo), MULTIBOOT_TAG_ALIGN)
		+ ALIGN_UP ((sizeof (struct multiboot_tag_mmap)
			+ ranges * sizeof (struct multiboot_mmap_entry)), MULTIBOOT_TAG_ALIGN)
		+ (sizeof (struct multiboot_tag_string)
			+ ALIGN_UP (cmdline_size, MULTIBOOT_TAG_ALIGN))
		+ (sizeof (struct multiboot_tag_string)
			+ ALIGN_UP (strlen(BOOTLOADER " " BOOTLOADER_VERSION) + 1, MULTIBOOT_TAG_ALIGN))
		+ (modcount * sizeof (struct multiboot_tag_module) + modcmd_size))
		+ sizeof (struct multiboot_tag); /* end tag */

	if (mhi.rel_tag)
		mbi_size += ALIGN_UP (sizeof (struct multiboot_tag_load_base_addr), MULTIBOOT_TAG_ALIGN);

	if (mhi.fb_tag)
		mbi_size += ALIGN_UP (sizeof (struct multiboot_tag_framebuffer), MULTIBOOT_TAG_ALIGN);

	return mbi_size;
}

static void multiboot2_read_header_tags(void)
{
	struct multiboot_header_tag *tag;

	for (tag = (struct multiboot_header_tag *) (mbh + 1);
	     tag->type != MULTIBOOT_TAG_TYPE_END;
	     tag = (struct multiboot_header_tag *) ((char *) tag + ALIGN_UP (tag->size, MULTIBOOT_TAG_ALIGN)))
	{
		switch (tag->type)
		{
		case MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST:
		{
			mhi.request_tag = (struct multiboot_header_tag_information_request *) tag;
			break;
		}
		case MULTIBOOT_HEADER_TAG_RELOCATABLE:
		{
			mhi.rel_tag = (struct multiboot_header_tag_relocatable *) tag;
			break;
		}
		case MULTIBOOT_HEADER_TAG_ADDRESS:
		{
			mhi.addr_tag = (struct multiboot_header_tag_address *) tag;
			break;
		}
		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS:
		{
			mhi.entry_addr_tag = (struct multiboot_header_tag_entry_address *) tag;
			break;
		}
		case MULTIBOOT_HEADER_TAG_CONSOLE_FLAGS:
		{
			mhi.console_tag = (struct multiboot_header_tag_console_flags *) tag;
			break;
		}
		case MULTIBOOT_HEADER_TAG_FRAMEBUFFER:
		{
			mhi.fb_tag = (struct multiboot_header_tag_framebuffer *) tag;
			break;
		}
		case MULTIBOOT_HEADER_TAG_MODULE_ALIGN:
		{
			mhi.mod_align_tag = (struct multiboot_header_tag_module_align *) tag;
			break;
		}
		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64:
		case MULTIBOOT_HEADER_TAG_EFI_BS:
			/* Ignoring EFI. */
			break;
		default:
		{
			if (!(tag->flags & MULTIBOOT_HEADER_TAG_OPTIONAL))
				fprintf(stderr, "unsupported tag: 0x%x", tag->type);
			break;
		}
		}
	}
}

struct multiboot_mmap_entry *multiboot_construct_memory_map(struct memory_range *range,
							    int ranges,
							    unsigned long long *mem_lower,
							    unsigned long long *mem_upper)
{
	struct multiboot_mmap_entry *entries;
	int i;

	*mem_lower = *mem_upper = 0;
	entries = xmalloc(ranges * sizeof(*entries));
	for (i = 0; i < ranges; i++) {
		entries[i].addr = range[i].start;
		entries[i].len = range[i].end - range[i].start + 1;

		if (range[i].type == RANGE_RAM) {
			entries[i].type = MULTIBOOT_MEMORY_AVAILABLE;
			/*
			 * Is this the "low" memory?  Can't just test
			 * against zero, because Linux protects (and
			 * hides) the first few pages of physical
			 * memory.
			 */

			if ((range[i].start <= 64*1024)
				&& (range[i].end > *mem_lower)) {
				range[i].start = 0;
				*mem_lower = range[i].end;
			}
			/* Is this the "high" memory? */
			if ((range[i].start <= 0x100000)
				&& (range[i].end > *mem_upper + 0x100000))
			*mem_upper = range[i].end - 0x100000;
		} else if (range[i].type == RANGE_ACPI)
			entries[i].type = MULTIBOOT_MEMORY_ACPI_RECLAIMABLE;
		else if (range[i].type == RANGE_ACPI_NVS)
			entries[i].type = MULTIBOOT_MEMORY_NVS;
		else if (range[i].type == RANGE_RESERVED)
			entries[i].type = MULTIBOOT_MEMORY_RESERVED;
	}
	return entries;
}

static uint64_t multiboot2_make_mbi(struct kexec_info *info, char *cmdline, int cmdline_len,
			     unsigned long load_base_addr, void *mbi_buf, size_t mbi_bytes)
{
	uint64_t *ptrorig = mbi_buf;
	struct multiboot_mmap_entry *mmap_entries;
	unsigned long long mem_lower = 0, mem_upper = 0;

	*ptrorig = mbi_bytes; /* u32 total_size, u32 reserved */
	ptrorig++;

	mmap_entries = multiboot_construct_memory_map(info->memory_range, info->memory_ranges, &mem_lower, &mem_upper);
	{
		struct multiboot_tag_basic_meminfo *tag = (struct multiboot_tag_basic_meminfo *) ptrorig;

		tag->type = MULTIBOOT_TAG_TYPE_BASIC_MEMINFO;
		tag->size = sizeof (struct multiboot_tag_basic_meminfo);
		tag->mem_lower = mem_lower >> 10;
		tag->mem_upper = mem_upper >> 10;
		ptrorig += ALIGN_UP (tag->size, MULTIBOOT_TAG_ALIGN) / sizeof (*ptrorig);
	}

	{
		struct multiboot_tag_mmap *tag = (struct multiboot_tag_mmap *) ptrorig;

		tag->type = MULTIBOOT_TAG_TYPE_MMAP;
		tag->size = sizeof(struct multiboot_tag_mmap) + sizeof(struct multiboot_mmap_entry) * info->memory_ranges;
		tag->entry_size = sizeof(struct multiboot_mmap_entry);
		tag->entry_version = 0;
		memcpy(tag->entries, mmap_entries, tag->entry_size * info->memory_ranges);
		ptrorig += ALIGN_UP (tag->size, MULTIBOOT_TAG_ALIGN) / sizeof (*ptrorig);
	}

	if (mhi.rel_tag) {
		struct multiboot_tag_load_base_addr *tag = (struct multiboot_tag_load_base_addr *) ptrorig;

		tag->type = MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR;
		tag->size = sizeof (struct multiboot_tag_load_base_addr);
		tag->load_base_addr = load_base_addr;
		ptrorig += ALIGN_UP (tag->size, MULTIBOOT_TAG_ALIGN) / sizeof (*ptrorig);
	}

	{
		struct multiboot_tag_string *tag = (struct multiboot_tag_string *) ptrorig;

		tag->type = MULTIBOOT_TAG_TYPE_CMDLINE;
		tag->size = sizeof (struct multiboot_tag_string) + cmdline_len;
		memcpy(tag->string, cmdline, cmdline_len);
		ptrorig += ALIGN_UP (tag->size, MULTIBOOT_TAG_ALIGN) / sizeof (*ptrorig);
	}

	{
		struct multiboot_tag_string *tag = (struct multiboot_tag_string *) ptrorig;

		tag->type = MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME;
		tag->size = sizeof(struct multiboot_tag_string) + strlen(BOOTLOADER " " BOOTLOADER_VERSION) + 1;
		sprintf(tag->string, "%s", BOOTLOADER " " BOOTLOADER_VERSION);
		ptrorig += ALIGN_UP (tag->size, MULTIBOOT_TAG_ALIGN) / sizeof (*ptrorig);
	}

	if (mhi.fb_tag) {
		struct multiboot_tag_framebuffer *tag = (struct multiboot_tag_framebuffer *) ptrorig;
		struct fb_fix_screeninfo info;
		struct fb_var_screeninfo mode;
		int fd;

		tag->common.type = MULTIBOOT_TAG_TYPE_FRAMEBUFFER;
		tag->common.size = sizeof(struct multiboot_tag_framebuffer);
		/* check if purgatory will reset to standard ega text mode */
		if (arch_options.reset_vga || arch_options.console_vga) {
			tag->common.framebuffer_type = MB_FRAMEBUFFER_TYPE_EGA_TEXT;
			tag->common.framebuffer_addr = 0xb8000;
			tag->common.framebuffer_pitch = 80*2;
			tag->common.framebuffer_width = 80;
			tag->common.framebuffer_height = 25;
			tag->common.framebuffer_bpp = 16;

			ptrorig += ALIGN_UP (tag->common.size, MULTIBOOT_TAG_ALIGN) / sizeof (*ptrorig);
			goto out;
		}

		/* use current graphics framebuffer settings */
		fd = open("/dev/fb0", O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "can't open /dev/fb0: %s\n", strerror(errno));
			goto out;
		}
		if (ioctl(fd, FBIOGET_FSCREENINFO, &info) < 0){
			fprintf(stderr, "can't get screeninfo: %s\n", strerror(errno));
			close(fd);
			goto out;
		}
		if (ioctl(fd, FBIOGET_VSCREENINFO, &mode) < 0){
			fprintf(stderr, "can't get modeinfo: %s\n", strerror(errno));
			close(fd);
			goto out;
		}
		close(fd);

		if (info.smem_start == 0 || info.smem_len == 0) {
			fprintf(stderr, "can't get linerar framebuffer address\n");
			goto out;
		}

		if (info.type != FB_TYPE_PACKED_PIXELS) {
			fprintf(stderr, "unsupported framebuffer type\n");
			goto out;
		}

		if (info.visual != FB_VISUAL_TRUECOLOR) {
			fprintf(stderr, "unsupported framebuffer visual\n");
			goto out;
		}

		tag->common.framebuffer_type = MB_FRAMEBUFFER_TYPE_RGB;
		tag->common.framebuffer_addr = info.smem_start;
		tag->common.framebuffer_pitch = info.line_length;
		tag->common.framebuffer_width = mode.xres;
		tag->common.framebuffer_height = mode.yres;
		tag->common.framebuffer_bpp = mode.bits_per_pixel;

		tag->framebuffer_red_field_position = mode.red.offset;
		tag->framebuffer_red_mask_size = mode.red.length;
		tag->framebuffer_green_field_position = mode.green.offset;
		tag->framebuffer_green_mask_size = mode.green.length;
		tag->framebuffer_blue_field_position = mode.blue.offset;
		tag->framebuffer_blue_mask_size = mode.blue.length;

		ptrorig += ALIGN_UP (tag->common.size, MULTIBOOT_TAG_ALIGN) / sizeof (*ptrorig);
	}

out:
	return (uint64_t) (uintptr_t) ptrorig;
}

static uint64_t multiboot2_mbi_add_module(void *mbi_buf, uint64_t mbi_ptr, uint32_t mod_start,
					  uint32_t mod_end, char *mod_clp)
{
	struct multiboot_tag_module *tag = (struct multiboot_tag_module *) (uintptr_t) mbi_ptr;

	tag->type = MULTIBOOT_TAG_TYPE_MODULE;
	tag->size = sizeof(struct multiboot_tag_module) + strlen((char *)(long) mod_clp) + 1;
	tag->mod_start = mod_start;
	tag->mod_end = mod_end;

	memcpy(tag->cmdline, (char *)(long) mod_clp, strlen((char *)(long) mod_clp) + 1);
	mbi_ptr += ALIGN_UP (tag->size, MULTIBOOT_TAG_ALIGN);

	return mbi_ptr;
}

static uint64_t multiboot2_mbi_end(void *mbi_buf, uint64_t mbi_ptr)
{
	struct multiboot_tag *tag = (struct multiboot_tag *) (uintptr_t) mbi_ptr;

	tag->type = MULTIBOOT_TAG_TYPE_END;
	tag->size = sizeof (struct multiboot_tag);
	mbi_ptr += ALIGN_UP (tag->size, MULTIBOOT_TAG_ALIGN);

	return mbi_ptr;
}

static inline int multiboot2_rel_valid(struct multiboot_header_tag_relocatable *rel_tag,
					uint64_t rel_start, uint64_t rel_end)
{
	if (rel_start >= rel_tag->min_addr && rel_end <= rel_tag->max_addr)
		return 1;

	return 0;
}

int multiboot2_x86_load(int argc, char **argv, const char *buf, off_t len,
			struct kexec_info *info)
{
	struct mem_ehdr ehdr;
	void *mbi_buf;
	size_t mbi_bytes;
	unsigned long addr;
	struct entry32_regs regs;
	char *command_line = NULL, *tmp_cmdline = NULL;
	int command_line_len;
	char *imagename, *cp, *append = NULL;;
	int opt;
	int modules, mod_command_line_space;
	uint64_t mbi_ptr;
	char *mod_clp_base;
	/* See options.h -- add any more there, too. */
	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{ "command-line",		1, 0, OPT_CL },
		{ "append",			1, 0, OPT_CL },
		{ "reuse-cmdline",		0, 0, OPT_REUSE_CMDLINE },
		{ "module",			1, 0, OPT_MOD },
		{ 0, 				0, 0, 0 },
	};
	static const char short_options[] = KEXEC_ARCH_OPT_STR "";
	uint64_t rel_min, rel_max;

	/* Probe for the MB header if it's not already found */
	if (mbh == NULL && multiboot_x86_probe(buf, len) != 1)
	{
		fprintf(stderr, "Cannot find a loadable multiboot2 header.\n");
		return -1;
	}

	/* Parse the header tags. */
	multiboot2_read_header_tags();

	/* Parse the command line */
	command_line_len = 0;
	modules = 0;
	mod_command_line_space = 0;
	while((opt = getopt_long(argc, argv, short_options, options, 0)) != -1)
	{
		switch(opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX) {
				break;
			}
		case OPT_CL:
			append = optarg;
			break;
		case OPT_REUSE_CMDLINE:
			tmp_cmdline = get_command_line();
			break;
		case OPT_MOD:
			modules++;
			mod_command_line_space += strlen(optarg) + 1;
			break;
		}
	}
	imagename = argv[optind];

	/* Final command line = imagename + <OPT_REUSE_CMDLINE> + <OPT_CL> */
	tmp_cmdline = concat_cmdline(command_line, append);
	if (command_line) {
		free(command_line);
	}
	command_line = concat_cmdline(imagename, tmp_cmdline);
	if (tmp_cmdline) {
		free(tmp_cmdline);
	}

	if (xen_present() && info->kexec_flags & KEXEC_LIVE_UPDATE ) {
		if (!mhi.rel_tag) {
			fprintf(stderr, "Multiboot2 image must be relocatable"
				"for KEXEC_LIVE_UPDATE.\n");
			return -1;
		}
		cmdline_add_liveupdate(&command_line);
	}

	command_line_len = strlen(command_line) + 1;

	/* Load the ELF executable */
	if (mhi.rel_tag) {
		rel_min = mhi.rel_tag->min_addr;
		rel_max = mhi.rel_tag->max_addr;

		if (info->kexec_flags & KEXEC_LIVE_UPDATE && xen_present()) {
			/* TODO also check if elf is xen */
			/* On a live update, load target xen over the current xen image. */
			uint64_t xen_start, xen_end;

			xen_get_kexec_range(KEXEC_RANGE_MA_XEN, &xen_start, &xen_end);
			if (multiboot2_rel_valid(mhi.rel_tag, xen_start, xen_end)) {
				rel_min = xen_start;
			} else {
				fprintf(stderr, "Cannot place Elf into "
				"KEXEC_RANGE_MA_XEN for KEXEC_LIVE_UPDATE.\n");
				return -1;
			}
		}

		elf_exec_build_load_relocatable(info, &ehdr, buf, len, 0,
						rel_min, rel_max, mhi.rel_tag->align);
	} else
		elf_exec_build_load(info, &ehdr, buf, len, 0);

	if (info->kexec_flags & KEXEC_LIVE_UPDATE && xen_present()) {
		uint64_t lu_start, lu_end;

		xen_get_kexec_range(7 /* KEXEC_RANGE_MA_LIVEUPDATE */, &lu_start, &lu_end);
		/* Fit everything else into lu_start-lu_end. First page after lu_start is
		 * reserved for LU breadcrumb. */
		rel_min = lu_start + 4096;
		rel_max = lu_end;
	} else {
		rel_min = 0x500;
		rel_max = ULONG_MAX;
	}

	/* Load the setup code */
	elf_rel_build_load(info, &info->rhdr, purgatory, purgatory_size,
			   rel_min, rel_max, 1, 0);

	/* Construct information tags. */
	mbi_bytes = multiboot2_get_mbi_size(info->memory_ranges, command_line_len, modules, mod_command_line_space);
	mbi_buf = xmalloc(mbi_bytes);

	mbi_ptr = multiboot2_make_mbi(info, command_line, command_line_len, info->rhdr.rel_addr, mbi_buf, mbi_bytes);
	free(command_line);

	if (info->kexec_flags & KEXEC_LIVE_UPDATE && xen_present()) {
		if (multiboot2_rel_valid(mhi.rel_tag, rel_min, rel_max)) {
			/* Shrink the reloc range to fit into LU region for xen. */
			mhi.rel_tag->min_addr = rel_min;
			mhi.rel_tag->max_addr = rel_max;
		} else {
			fprintf(stderr, "Multiboot2 image cannot be relocated into "
				"KEXEC_RANGE_MA_LIVEUPDATE for KEXEC_LIVE_UPDATE.\n");
			return -1;
		}
	}

	/* Load modules */
	if (modules) {
		char *mod_filename, *mod_command_line, *mod_clp, *buf;
		off_t mod_size;
		int i = 0;

		mod_clp_base = xmalloc(mod_command_line_space);

		/* Go back and parse the module command lines */
		mod_clp = mod_clp_base;
		optind = opterr = 1;
		while((opt = getopt_long(argc, argv,
					 short_options, options, 0)) != -1) {
			if (opt != OPT_MOD) continue;

			/* Split module filename from command line */
			mod_command_line = mod_filename = optarg;
			if ((cp = strchr(mod_filename, ' ')) != NULL) {
				/* See as I discard the 'const' modifier */
				*cp = '\0';
			}

			/* Load the module */
			buf = slurp_decompress_file(mod_filename, &mod_size);

			if (cp != NULL) *cp = ' ';

			/* Pick the next aligned spot to load it in. Always page align. */
			addr = add_buffer(info, buf, mod_size, mod_size, getpagesize(),
					  rel_min, rel_max, 1);

			/* Add the module command line */
			sprintf(mod_clp, "%s", mod_command_line);

			mbi_ptr = multiboot2_mbi_add_module(mbi_buf, mbi_ptr, addr, addr + mod_size, mod_clp);

			mod_clp += strlen(mod_clp) + 1;
			i++;
		}

		free(mod_clp_base);
	}

	mbi_ptr = multiboot2_mbi_end(mbi_buf, mbi_ptr);

	if (sort_segments(info) < 0)
		return -1;

	addr = add_buffer(info, mbi_buf, mbi_bytes, mbi_bytes, 4,
			  rel_min, rel_max, 1);

	elf_rel_get_symbol(&info->rhdr, "entry32_regs", &regs, sizeof(regs));
	regs.eax = MULTIBOOT2_BOOTLOADER_MAGIC;
	regs.ebx = addr;
	regs.eip = ehdr.e_entry;
	elf_rel_set_symbol(&info->rhdr, "entry32_regs", &regs, sizeof(regs));

	return 0;
}
