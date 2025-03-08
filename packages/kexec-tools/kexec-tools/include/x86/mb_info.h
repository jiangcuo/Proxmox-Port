/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000  Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 *  The structure type "mod_list" is used by the "multiboot_info" structure.
 */

struct mod_list
{
	/* the memory used goes from bytes 'mod_start' to 'mod_end-1' inclusive */
	uint32_t mod_start;
	uint32_t mod_end;
	
	/* Module command line */
	uint32_t cmdline;
  
	/* padding to take it to 16 bytes (must be zero) */
	uint32_t pad;
};


/*
 *  INT-15, AX=E820 style "AddressRangeDescriptor"
 *  ...with a "size" parameter on the front which is the structure size - 4,
 *  pointing to the next one, up until the full buffer length of the memory
 *  map has been reached.
 */

struct AddrRangeDesc
{
	uint32_t size;
	uint32_t base_addr_low;
	uint32_t base_addr_high;
	uint32_t length_low;
	uint32_t length_high;
	uint32_t Type;
  
  /* unspecified optional padding... */
};

/* usable memory "Type", all others are reserved.  */
#define MB_ARD_MEMORY		1


/* Drive Info structure.  */
struct drive_info
{
	/* The size of this structure.  */
	uint32_t size;

	/* The BIOS drive number.  */
	uint8_t drive_number;
	
	/* The access mode (see below).  */
	uint8_t drive_mode;
	
	/* The BIOS geometry.  */
	uint16_t drive_cylinders;
	uint8_t drive_heads;
	uint8_t drive_sectors;
	
	/* The array of I/O ports used for the drive.  */
	uint16_t drive_ports[0];
};

/* Drive Mode.  */
#define MB_DI_CHS_MODE		0
#define MB_DI_LBA_MODE		1


/* APM BIOS info.  */
struct apm_info
{
	uint16_t version;
	uint16_t cseg;
	uint32_t offset;
	uint32_t cseg_16;
	uint32_t dseg_16;
	uint32_t cseg_len;
	uint32_t cseg_16_len;
	uint32_t dseg_16_len;
};


/*
 *  MultiBoot Info description
 *
 *  This is the struct passed to the boot image.  This is done by placing
 *  its address in the EAX register.
 */

struct multiboot_info
{
	/* MultiBoot info version number */
	uint32_t flags;
	
	/* Available memory from BIOS */
	uint32_t mem_lower;
	uint32_t mem_upper;
	
	/* "root" partition */
	uint32_t boot_device;
	
	/* Kernel command line */
	uint32_t cmdline;
	
	/* Boot-Module list */
	uint32_t mods_count;
	uint32_t mods_addr;
	
	union
	{
		struct
		{
			/* (a.out) Kernel symbol table info */
			uint32_t tabsize;
			uint32_t strsize;
			uint32_t addr;
			uint32_t pad;
		}
		a;
		
		struct
		{
			/* (ELF) Kernel section header table */
			uint32_t num;
			uint32_t size;
			uint32_t addr;
			uint32_t shndx;
		}
		e;
	}
	syms;
	
	/* Memory Mapping buffer */
	uint32_t mmap_length;
	uint32_t mmap_addr;
	
	/* Drive Info buffer */
	uint32_t drives_length;
	uint32_t drives_addr;
	
	/* ROM configuration table */
	uint32_t config_table;
	
	/* Boot Loader Name */
	uint32_t boot_loader_name;
	
	/* APM table */
	uint32_t apm_table;
	
	/* Video */
	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint16_t vbe_mode;
	uint16_t vbe_interface_seg;
	uint16_t vbe_interface_off;
	uint16_t vbe_interface_len;

	uint64_t framebuffer_addr;
	uint32_t framebuffer_pitch;
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint8_t  framebuffer_bpp;
	uint8_t	 framebuffer_type;

	union {
		struct {
			uint32_t	framebuffer_palette_addr;
			uint16_t	framebuffer_palette_num_color;
		};
		struct {
			uint8_t		framebuffer_red_field_position;
			uint8_t		framebuffer_red_mask_size;
			uint8_t		framebuffer_green_field_position;
			uint8_t		framebuffer_green_mask_size;
			uint8_t		framebuffer_blue_field_position;
			uint8_t		framebuffer_blue_mask_size;
		};
	};
};

/*
 *  Flags to be set in the 'flags' parameter above
 */

/* is there basic lower/upper memory information? */
#define MB_INFO_MEMORY			0x00000001
/* is there a boot device set? */
#define MB_INFO_BOOTDEV			0x00000002
/* is the command-line defined? */
#define MB_INFO_CMDLINE			0x00000004
/* are there modules to do something with? */
#define MB_INFO_MODS			0x00000008

/* These next two are mutually exclusive */

/* is there a symbol table loaded? */
#define MB_INFO_AOUT_SYMS		0x00000010
/* is there an ELF section header table? */
#define MB_INFO_ELF_SHDR		0x00000020

/* is there a full memory map? */
#define MB_INFO_MEM_MAP			0x00000040

/* Is there drive info?  */
#define MB_INFO_DRIVE_INFO		0x00000080

/* Is there a config table?  */
#define MB_INFO_CONFIG_TABLE		0x00000100

/* Is there a boot loader name?  */
#define MB_INFO_BOOT_LOADER_NAME	0x00000200

/* Is there a APM table?  */
#define MB_INFO_APM_TABLE		0x00000400

/* Is there video information?  */
#define MB_INFO_VIDEO_INFO		0x00000800
#define MB_INFO_FRAMEBUFFER_INFO	0x00001000

#define MB_FRAMEBUFFER_TYPE_INDEXED	0
#define MB_FRAMEBUFFER_TYPE_RGB		1
#define MB_FRAMEBUFFER_TYPE_EGA_TEXT	2

/*
 *  The following value must be present in the EAX register.
 */

#define MULTIBOOT_VALID			0x2BADB002
