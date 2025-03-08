/*
 * purgatory:  setup code
 *
 * Copyright (C) 2005-2006  Zou Nan hai (nanhai.zou@intel.com)
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
#include <purgatory.h>
#include <stdint.h>
#include <string.h>
#include "purgatory-ia64.h"

#define PAGE_OFFSET             0xe000000000000000UL

#define EFI_PAGE_SHIFT          12
#define EFI_PAGE_SIZE		(1UL<<EFI_PAGE_SHIFT)
#define EFI_PAGE_ALIGN(x)	((x + EFI_PAGE_SIZE - 1)&~(EFI_PAGE_SIZE-1))
/* Memory types: */
#define EFI_RESERVED_TYPE                0
#define EFI_LOADER_CODE                  1
#define EFI_LOADER_DATA                  2
#define EFI_BOOT_SERVICES_CODE           3
#define EFI_BOOT_SERVICES_DATA           4
#define EFI_RUNTIME_SERVICES_CODE        5
#define EFI_RUNTIME_SERVICES_DATA        6
#define EFI_CONVENTIONAL_MEMORY          7
#define EFI_UNUSABLE_MEMORY              8
#define EFI_ACPI_RECLAIM_MEMORY          9
#define EFI_ACPI_MEMORY_NVS             10
#define EFI_MEMORY_MAPPED_IO            11
#define EFI_MEMORY_MAPPED_IO_PORT_SPACE 12
#define EFI_PAL_CODE                    13
#define EFI_MAX_MEMORY_TYPE             14

typedef struct {
        uint64_t signature;
        uint32_t revision;
        uint32_t headersize;
        uint32_t crc32;
        uint32_t reserved;
} efi_table_hdr_t;

typedef struct {
        efi_table_hdr_t hdr;
        unsigned long get_time;
        unsigned long set_time;
        unsigned long get_wakeup_time;
        unsigned long set_wakeup_time;
        unsigned long set_virtual_address_map;
        unsigned long convert_pointer;
        unsigned long get_variable;
        unsigned long get_next_variable;
        unsigned long set_variable;
        unsigned long get_next_high_mono_count;
        unsigned long reset_system;
} efi_runtime_services_t;

typedef struct {
        efi_table_hdr_t hdr;
        unsigned long fw_vendor;          /* physical addr of
					     CHAR16 vendor string */
        uint32_t fw_revision;
        unsigned long con_in_handle;
        unsigned long con_in;
        unsigned long con_out_handle;
        unsigned long con_out;
        unsigned long stderr_handle;
        unsigned long stderr;
        unsigned long runtime;
        unsigned long boottime;
        unsigned long nr_tables;
        unsigned long tables;
} efi_system_table_t;

struct ia64_boot_param {
        uint64_t command_line;             /* physical address of
					      command linearguments */
        uint64_t efi_systab;               /* physical address of
					      EFI system table */
        uint64_t efi_memmap;               /* physical address of
					      EFI memory map */
        uint64_t efi_memmap_size;          /* size of EFI memory map */
        uint64_t efi_memdesc_size;         /* size of an EFI memory map
					      descriptor */
        uint32_t efi_memdesc_version;      /* memory descriptor version */
        struct {
                uint16_t num_cols;         /* number of columns on console
					      output device */
                uint16_t num_rows;         /* number of rows on console
					      output device */
                uint16_t orig_x;           /* cursor's x position */
                uint16_t orig_y;           /* cursor's y position */
        } console_info;
        uint64_t fpswa;                    /* physical address of
					      the fpswa interface */
        uint64_t initrd_start;
        uint64_t initrd_size;

        uint64_t vmcode_start;
        uint64_t vmcode_size;
};

typedef struct {
        uint32_t type;
        uint32_t pad;
        uint64_t phys_addr;
        uint64_t virt_addr;
        uint64_t num_pages;
        uint64_t attribute;
} efi_memory_desc_t;

struct loaded_segment {
        unsigned long start;
        unsigned long end;
};

struct kexec_boot_params {
	uint64_t vmcode_base;
	uint64_t vmcode_size;
	uint64_t ramdisk_base;
	uint64_t ramdisk_size;
	uint64_t command_line;
	uint64_t command_line_len;
	uint64_t efi_memmap_base;
	uint64_t efi_memmap_size;
	uint64_t boot_param_base;
	struct loaded_segment *loaded_segments;
	unsigned long loaded_segments_num;
};

void
setup_arch(void)
{
	reset_vga();
}

inline unsigned long PA(unsigned long addr)
{
	return addr & 0x0fffffffffffffffLL;
}

void
patch_efi_memmap(struct kexec_boot_params *params,
		struct ia64_boot_param *boot_param)
{
	void *dest = (void *)params->efi_memmap_base;
	void *src  = (void *)boot_param->efi_memmap;
	uint64_t orig_type;
	efi_memory_desc_t *src_md, *dst_md;
	void *src_end = src + boot_param->efi_memmap_size;
	unsigned long i;
	for (; src < src_end; src += boot_param->efi_memdesc_size,
	     dest += boot_param->efi_memdesc_size) {
		unsigned long mstart, mend;
		src_md = src;
		dst_md = dest;
		if (src_md->num_pages == 0)
			continue;
		mstart = src_md->phys_addr;
		mend = src_md->phys_addr +
			(src_md->num_pages << EFI_PAGE_SHIFT);
		*dst_md = *src_md;
		if (src_md->type == EFI_LOADER_DATA)
			dst_md->type = EFI_CONVENTIONAL_MEMORY;
		/* segments are already sorted and aligned to 4K */
		orig_type = dst_md->type;
		for (i = 0; i < params->loaded_segments_num; i++) {
			struct loaded_segment *seg;
			unsigned long start_pages, mid_pages, end_pages;

			seg = &params->loaded_segments[i];
			if (seg->start < mstart || seg->start >= mend)
				continue;

			while (seg->end > mend && src < src_end) {
				src += boot_param->efi_memdesc_size;
				src_md = src;
				/* TODO check contig and attribute here */
				mend = src_md->phys_addr +
					(src_md->num_pages << EFI_PAGE_SHIFT);
			}
                        if (seg->end < mend && src < src_end) {
                                void *src_next;
                                efi_memory_desc_t *src_next_md;
                                src_next = src + boot_param->efi_memdesc_size;
                                src_next_md = src_next;
                                if (src_next_md->type ==
                                    EFI_CONVENTIONAL_MEMORY) {
				        /* TODO check contig and attribute */
                                        src += boot_param->efi_memdesc_size;
                                        src_md = src;
				        mend = src_md->phys_addr +
					        (src_md->num_pages <<
                                                 EFI_PAGE_SHIFT);
                                }

                        }
			start_pages = (seg->start - mstart) >> EFI_PAGE_SHIFT;
			mid_pages = (seg->end - seg->start) >> EFI_PAGE_SHIFT;
			end_pages = (mend - seg->end) >> EFI_PAGE_SHIFT;
			if (start_pages) {
				dst_md->num_pages = start_pages;
				dest += boot_param->efi_memdesc_size;
				dst_md = dest;
				*dst_md = *src_md;
			}
			dst_md->phys_addr = seg->start;
			dst_md->num_pages = mid_pages;
			dst_md->type = EFI_LOADER_DATA;
			if (!end_pages)
				break;
			dest += boot_param->efi_memdesc_size;
			dst_md = dest;
			*dst_md = *src_md;
			dst_md->phys_addr = seg->end;
			dst_md->num_pages = end_pages;
			dst_md->type = orig_type;
			mstart = seg->end;
		}
	}

	boot_param->efi_memmap_size = dest - (void *)params->efi_memmap_base;
}

void
flush_icache_range(char *start, unsigned long len)
{
	unsigned long i, addr;
	addr = (unsigned long)start & ~31UL;
	len += (unsigned long)start - addr;
	for (i = 0;i < len; i += 32)
	  asm volatile("fc.i %0"::"r"(start + i):"memory");
	asm volatile (";;sync.i;;":::"memory");
	asm volatile ("srlz.i":::"memory");
}

extern char __dummy_efi_function[], __dummy_efi_function_end[];


void
ia64_env_setup(struct ia64_boot_param *boot_param,
	struct kexec_boot_params *params)
{
	unsigned long len;
        efi_system_table_t *systab;
        efi_runtime_services_t *runtime;
	unsigned long *set_virtual_address_map;
	char *command_line = (char *)params->command_line;
	uint64_t command_line_len = params->command_line_len;
	struct ia64_boot_param *new_boot_param =
	(struct ia64_boot_param *) params->boot_param_base;
	memcpy(new_boot_param, boot_param, 4096);

	/*
	 * patch efi_runtime->set_virtual_address_map to a dummy function
	 *
	 * The EFI specification mandates that set_virtual_address_map only
	 * takes effect the first time that it is called, and that
	 * subsequent calls will return error.  By replacing it with a
	 * dummy function the new OS can think it is calling it again
	 * without either the OS or any buggy EFI implementations getting
	 * upset.
	 *
	 * Note: as the EFI specification says that set_virtual_address_map
	 * will only take affect the first time it is called, the mapping
	 * can't be updated, and thus mapping of the old and new OS really
	 * needs to be the same.
	 */
	len = __dummy_efi_function_end - __dummy_efi_function;
	memcpy(command_line + command_line_len,
		__dummy_efi_function, len);
	systab = (efi_system_table_t *)new_boot_param->efi_systab;
	runtime = (efi_runtime_services_t *)PA(systab->runtime);
	set_virtual_address_map =
		(unsigned long *)PA(runtime->set_virtual_address_map);
	*(set_virtual_address_map) =
		(unsigned long)(command_line + command_line_len);
	flush_icache_range(command_line + command_line_len, len);

	patch_efi_memmap(params, new_boot_param);

	new_boot_param->efi_memmap = params->efi_memmap_base;
	new_boot_param->command_line = params->command_line;
	new_boot_param->console_info.orig_x = 0;
	new_boot_param->console_info.orig_y = 0;
	new_boot_param->initrd_start = params->ramdisk_base;
	new_boot_param->initrd_size =  params->ramdisk_size;
	new_boot_param->vmcode_start = params->vmcode_base;
	new_boot_param->vmcode_size =  params->vmcode_size;
}

/* This function can be used to execute after the SHA256 verification. */
void post_verification_setup_arch(void)
{
	/* Nothing for now */
}
