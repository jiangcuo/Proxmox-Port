/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2003,2004  Eric Biederman (ebiederm@xmission.com)
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
 */
#define _GNU_SOURCE
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/screen_info.h>
#include <unistd.h>
#include <dirent.h>
#include <mntent.h>
#include <x86/x86-linux.h>
#include "../../kexec.h"
#include "kexec-x86.h"
#include "x86-linux-setup.h"
#include "../../kexec/kexec-syscall.h"

#ifndef VIDEO_CAPABILITY_64BIT_BASE
#define VIDEO_CAPABILITY_64BIT_BASE (1 << 1)	/* Frame buffer base is 64-bit */
#endif

void init_linux_parameters(struct x86_linux_param_header *real_mode)
{
	/* Fill in the values that are usually provided by the kernel. */

	/* Boot block magic */
	memcpy(real_mode->header_magic, "HdrS", 4);
	real_mode->protocol_version = 0x0206;
	real_mode->initrd_addr_max = DEFAULT_INITRD_ADDR_MAX;
	real_mode->cmdline_size = COMMAND_LINE_SIZE;
}

void setup_linux_bootloader_parameters_high(
	struct kexec_info *info, struct x86_linux_param_header *real_mode,
	unsigned long real_mode_base, unsigned long cmdline_offset,
	const char *cmdline, off_t cmdline_len,
	const char *initrd_buf, off_t initrd_size, int initrd_high)
{
	char *cmdline_ptr;
	unsigned long initrd_base, initrd_addr_max;

	/* Say I'm a boot loader */
	real_mode->loader_type = LOADER_TYPE_KEXEC << 4;

	/* No loader flags */
	real_mode->loader_flags = 0;

	/* Find the maximum initial ramdisk address */
	if (initrd_high)
		initrd_addr_max = ULONG_MAX;
	else {
		initrd_addr_max = DEFAULT_INITRD_ADDR_MAX;
		if (real_mode->protocol_version >= 0x0203) {
			initrd_addr_max = real_mode->initrd_addr_max;
			dbgprintf("initrd_addr_max is 0x%lx\n",
					 initrd_addr_max);
		}
	}

	/* Load the initrd if we have one */
	if (initrd_buf) {
		initrd_base = add_buffer(info,
			initrd_buf, initrd_size, initrd_size,
			4096, INITRD_BASE, initrd_addr_max, -1);
		dbgprintf("Loaded initrd at 0x%lx size 0x%lx\n", initrd_base,
			initrd_size);
	} else {
		initrd_base = 0;
		initrd_size = 0;
	}

	/* Ramdisk address and size */
	real_mode->initrd_start = initrd_base & 0xffffffffUL;
	real_mode->initrd_size  = initrd_size & 0xffffffffUL;

	if (real_mode->protocol_version >= 0x020c &&
	    (initrd_base & 0xffffffffUL) != initrd_base)
		real_mode->ext_ramdisk_image = initrd_base >> 32;

	if (real_mode->protocol_version >= 0x020c &&
	    (initrd_size & 0xffffffffUL) != initrd_size)
		real_mode->ext_ramdisk_size = initrd_size >> 32;

	/* The location of the command line */
	/* if (real_mode_base == 0x90000) { */
		real_mode->cl_magic = CL_MAGIC_VALUE;
		real_mode->cl_offset = cmdline_offset;
		/* setup_move_size */
	/* } */
	if (real_mode->protocol_version >= 0x0202) {
		unsigned long cmd_line_ptr = real_mode_base + cmdline_offset;

		real_mode->cmd_line_ptr = cmd_line_ptr & 0xffffffffUL;
		if ((real_mode->protocol_version >= 0x020c) &&
		    ((cmd_line_ptr & 0xffffffffUL) != cmd_line_ptr))
			real_mode->ext_cmd_line_ptr = cmd_line_ptr >> 32;
	}

	/* Fill in the command line */
	if (cmdline_len > COMMAND_LINE_SIZE) {
		cmdline_len = COMMAND_LINE_SIZE;
	}
	cmdline_ptr = ((char *)real_mode) + cmdline_offset;
	memcpy(cmdline_ptr, cmdline, cmdline_len);
	cmdline_ptr[cmdline_len - 1] = '\0';
}

static int setup_linux_vesafb(struct x86_linux_param_header *real_mode)
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	int fd;

	fd = open("/dev/fb0", O_RDONLY);
	if (-1 == fd)
		return -1;

	if (-1 == ioctl(fd, FBIOGET_FSCREENINFO, &fix))
		goto out;
	if (-1 == ioctl(fd, FBIOGET_VSCREENINFO, &var))
		goto out;
	if (0 == strcmp(fix.id, "VESA VGA")) {
		/* VIDEO_TYPE_VLFB */
		real_mode->orig_video_isVGA = 0x23;
	} else if (0 == strcmp(fix.id, "EFI VGA")) {
		/* VIDEO_TYPE_EFI */
		real_mode->orig_video_isVGA = 0x70;
	} else if (arch_options.reuse_video_type) {
		int err;
		off_t offset = offsetof(typeof(*real_mode), orig_video_isVGA);

		/* blindly try old boot time video type */
		err = get_bootparam(&real_mode->orig_video_isVGA, offset, 1);
		if (err)
			goto out;
	} else {
		real_mode->orig_video_isVGA = 0;
		close(fd);
		return 0;
	}
	close(fd);

	real_mode->lfb_width      = var.xres;
	real_mode->lfb_height     = var.yres;
	real_mode->lfb_depth      = var.bits_per_pixel;
	real_mode->lfb_base       = fix.smem_start & 0xffffffffUL;
	real_mode->lfb_linelength = fix.line_length;
	real_mode->vesapm_seg     = 0;

	if (fix.smem_start > 0xffffffffUL) {
		real_mode->ext_lfb_base = fix.smem_start >> 32;
		real_mode->capabilities |= VIDEO_CAPABILITY_64BIT_BASE;
	}

	/* FIXME: better get size from the file returned by proc_iomem() */
	real_mode->lfb_size       = (fix.smem_len + 65535) / 65536;
	real_mode->pages          = (fix.smem_len + 4095) / 4096;

	if (var.bits_per_pixel > 8) {
		real_mode->red_pos    = var.red.offset;
		real_mode->red_size   = var.red.length;
		real_mode->green_pos  = var.green.offset;
		real_mode->green_size = var.green.length;
		real_mode->blue_pos   = var.blue.offset;
		real_mode->blue_size  = var.blue.length;
		real_mode->rsvd_pos   = var.transp.offset;
		real_mode->rsvd_size  = var.transp.length;
	}
	return 0;

 out:
	close(fd);
	return -1;
}

#define EDD_SYFS_DIR "/sys/firmware/edd"

#define EDD_EXT_FIXED_DISK_ACCESS           (1 << 0)
#define EDD_EXT_DEVICE_LOCKING_AND_EJECTING (1 << 1)
#define EDD_EXT_ENHANCED_DISK_DRIVE_SUPPORT (1 << 2)
#define EDD_EXT_64BIT_EXTENSIONS            (1 << 3)

/*
 * Scans one line from a given filename. Returns on success the number of
 * items written (same like scanf()).
 */
static int file_scanf(const char *dir, const char *file, const char *scanf_line, ...)
{
	va_list argptr;
	FILE *fp;
	int retno;
	char filename[PATH_MAX];

	snprintf(filename, PATH_MAX, "%s/%s", dir, file);
	filename[PATH_MAX-1] = 0;

	fp = fopen(filename, "r");
	if (!fp) {
		return -errno;
	}

	va_start(argptr, scanf_line);
	retno = vfscanf(fp, scanf_line, argptr);
	va_end(argptr);

	fclose(fp);

	return retno;
}

static int parse_edd_extensions(const char *dir, struct edd_info *edd_info)
{
	char filename[PATH_MAX];
	char line[1024];
	uint16_t flags = 0;
	FILE *fp;
	int ret;

	ret = snprintf(filename, PATH_MAX, "%s/%s", dir, "extensions");
	if (ret < 0 || ret >= PATH_MAX) {
		fprintf(stderr, "snprintf failed: %s\n", strerror(errno));
		return -1;
	}

	filename[PATH_MAX-1] = 0;

	fp = fopen(filename, "r");
	if (!fp) {
		return -errno;
	}

	while (fgets(line, 1024, fp)) {
		/*
		 * strings are in kernel source, function edd_show_extensions()
		 * drivers/firmware/edd.c
		 */
		if (strstr(line, "Fixed disk access") == line)
			flags |= EDD_EXT_FIXED_DISK_ACCESS;
		else if (strstr(line, "Device locking and ejecting") == line)
			flags |= EDD_EXT_DEVICE_LOCKING_AND_EJECTING;
		else if (strstr(line, "Enhanced Disk Drive support") == line)
			flags |= EDD_EXT_ENHANCED_DISK_DRIVE_SUPPORT;
		else if (strstr(line, "64-bit extensions") == line)
			flags |= EDD_EXT_64BIT_EXTENSIONS;
	}

	fclose(fp);

	edd_info->interface_support = flags;

	return 0;
}

static int read_edd_raw_data(const char *dir, struct edd_info *edd_info)
{
	char filename[PATH_MAX];
	FILE *fp;
	size_t read_chars;
	uint16_t len;
	int ret;

	ret = snprintf(filename, PATH_MAX, "%s/%s", dir, "raw_data");
	if (ret < 0 || ret >= PATH_MAX) {
		fprintf(stderr, "snprintf failed: %s\n", strerror(errno));
		return -1;
	}

	filename[PATH_MAX-1] = 0;

	fp = fopen(filename, "r");
	if (!fp) {
		return -errno;
	}

	memset(edd_info->edd_device_params, 0, EDD_DEVICE_PARAM_SIZE);
	read_chars = fread(edd_info->edd_device_params, sizeof(uint8_t),
				EDD_DEVICE_PARAM_SIZE, fp);
	fclose(fp);

	len = ((uint16_t *)edd_info->edd_device_params)[0];
	dbgprintf("EDD raw data has length %d\n", len);

	if (read_chars < len) {
		fprintf(stderr, "BIOS reported EDD length of %hd but only "
			"%d chars read.\n", len, (int)read_chars);
		return -1;
	}

	return 0;
}

static int add_edd_entry(struct x86_linux_param_header *real_mode,
		const char *sysfs_name, int *current_edd, int *current_mbr)
{
	uint8_t devnum, version;
	uint32_t mbr_sig;
	struct edd_info *edd_info;

	if (!current_mbr || !current_edd) {
		fprintf(stderr, "%s: current_edd and current_edd "
				"must not be NULL", __FUNCTION__);
		return -1;
	}

	edd_info = &real_mode->eddbuf[*current_edd];
	memset(edd_info, 0, sizeof(struct edd_info));

	/* extract the device number */
	char* sysfs_name_copy = strdup(sysfs_name);
	if (sscanf(basename(sysfs_name_copy), "int13_dev%hhx", &devnum) != 1) {
		fprintf(stderr, "Invalid format of int13_dev dir "
				"entry: %s\n", basename(sysfs_name_copy));
		free(sysfs_name_copy);
		return -1;
	}
	free(sysfs_name_copy);
	/* if there's a MBR signature, then add it */
	if (file_scanf(sysfs_name, "mbr_signature", "0x%x", &mbr_sig) == 1) {
		real_mode->edd_mbr_sig_buffer[*current_mbr] = mbr_sig;
		(*current_mbr)++;
		dbgprintf("EDD Device 0x%x: mbr_sig=0x%x\n", devnum, mbr_sig);
	}

	/* set the device number */
	edd_info->device = devnum;

	/* set the version */
	if (file_scanf(sysfs_name, "version", "0x%hhx", &version) != 1)
		return -1;

	edd_info->version = version;

	/* if version == 0, that's some kind of dummy entry */
	if (version != 0) {
		/* legacy_max_cylinder */
		if (file_scanf(sysfs_name, "legacy_max_cylinder", "%hu",
					&edd_info->legacy_max_cylinder) != 1) {
			fprintf(stderr, "Reading legacy_max_cylinder failed.\n");
			return -1;
		}

		/* legacy_max_head */
		if (file_scanf(sysfs_name, "legacy_max_head", "%hhu",
					&edd_info->legacy_max_head) != 1) {
			fprintf(stderr, "Reading legacy_max_head failed.\n");
			return -1;
		}

		/* legacy_sectors_per_track */
		if (file_scanf(sysfs_name, "legacy_sectors_per_track", "%hhu",
					&edd_info->legacy_sectors_per_track) != 1) {
			fprintf(stderr, "Reading legacy_sectors_per_track failed.\n");
			return -1;
		}

		/* Parse the EDD extensions */
		if (parse_edd_extensions(sysfs_name, edd_info) != 0) {
			fprintf(stderr, "Parsing EDD extensions failed.\n");
			return -1;
		}

		/* Parse the raw info */
		if (read_edd_raw_data(sysfs_name, edd_info) != 0) {
			fprintf(stderr, "Reading EDD raw data failed.\n");
			return -1;
		}
	}

	(*current_edd)++;

	return 0;
}

static void zero_edd(struct x86_linux_param_header *real_mode)
{
	real_mode->eddbuf_entries = 0;
	real_mode->edd_mbr_sig_buf_entries = 0;
	memset(real_mode->eddbuf, 0,
		EDDMAXNR * sizeof(struct edd_info));
	memset(real_mode->edd_mbr_sig_buffer, 0,
		EDD_MBR_SIG_MAX * sizeof(uint32_t));
}

void setup_edd_info(struct x86_linux_param_header *real_mode)
{
	DIR *edd_dir;
	struct dirent *cursor;
	int current_edd = 0;
	int current_mbr = 0;

	edd_dir = opendir(EDD_SYFS_DIR);
	if (!edd_dir) {
		dbgprintf(EDD_SYFS_DIR " does not exist.\n");
		return;
	}

	zero_edd(real_mode);
	while ((cursor = readdir(edd_dir))) {
		char full_dir_name[PATH_MAX];

		/* only read the entries that start with "int13_dev" */
		if (strstr(cursor->d_name, "int13_dev") != cursor->d_name)
			continue;

		snprintf(full_dir_name, PATH_MAX, "%s/%s",
				EDD_SYFS_DIR, cursor->d_name);
		full_dir_name[PATH_MAX-1] = 0;

		if (add_edd_entry(real_mode, full_dir_name, &current_edd,
					&current_mbr) != 0) {
			zero_edd(real_mode);
			goto out;
		}
	}

	real_mode->eddbuf_entries = current_edd;
	real_mode->edd_mbr_sig_buf_entries = current_mbr;

out:
	closedir(edd_dir);

	dbgprintf("Added %d EDD MBR entries and %d EDD entries.\n",
		real_mode->edd_mbr_sig_buf_entries,
		real_mode->eddbuf_entries);
}

/*
 * This really only makes sense for virtual filesystems that are only expected
 * to be mounted once (sysfs, debugsfs, proc), as it will return the first
 * instance listed in /proc/mounts, falling back to mtab if absent.
 * We search by type and not by name because the name can be anything;
 * while setting the name equal to the mount point is common, it cannot be
 * relied upon, as even kernel documentation examples recommends using
 * "none" as the name e.g. for debugfs.
 */
char *find_mnt_by_type(char *type)
{
	FILE *mtab;
	struct mntent *mnt;
	char *mntdir;

	mtab = setmntent("/proc/mounts", "r");
	if (!mtab) {
		// Fall back to mtab
		mtab = setmntent("/etc/mtab", "r");
	}
	if (!mtab)
		return NULL;
	for(mnt = getmntent(mtab); mnt; mnt = getmntent(mtab)) {
		if (strcmp(mnt->mnt_type, type) == 0)
			break;
	}
	mntdir = mnt ? strdup(mnt->mnt_dir) : NULL;
	endmntent(mtab);
	return mntdir;
}

int get_bootparam(void *buf, off_t offset, size_t size)
{
	int data_file;
	char *debugfs_mnt, *sysfs_mnt;
	char filename[PATH_MAX];
	int err, has_sysfs_params = 0;

	sysfs_mnt = find_mnt_by_type("sysfs");
	if (sysfs_mnt) {
		snprintf(filename, PATH_MAX, "%s/%s", sysfs_mnt,
			"kernel/boot_params/data");
		free(sysfs_mnt);
		err = access(filename, F_OK);
		if (!err)
			has_sysfs_params = 1;
	}

	if (!has_sysfs_params) {
		debugfs_mnt = find_mnt_by_type("debugfs");
		if (!debugfs_mnt)
			return 1;
		snprintf(filename, PATH_MAX, "%s/%s", debugfs_mnt,
				"boot_params/data");
		free(debugfs_mnt);
	}

	data_file = open(filename, O_RDONLY);
	if (data_file < 0)
		return 1;
	if (lseek(data_file, offset, SEEK_SET) < 0)
		goto close;
	read(data_file, buf, size);
close:
	close(data_file);
	return 0;
}

void setup_subarch(struct x86_linux_param_header *real_mode)
{
	off_t offset = offsetof(typeof(*real_mode), hardware_subarch);

	get_bootparam(&real_mode->hardware_subarch, offset, sizeof(uint32_t));
}

struct efi_mem_descriptor {
	uint32_t type;
	uint32_t pad;
	uint64_t phys_addr;
	uint64_t virt_addr;
	uint64_t num_pages;
	uint64_t attribute;
};

struct efi_setup_data {
	uint64_t fw_vendor;
	uint64_t runtime;
	uint64_t tables;
	uint64_t smbios;
	uint64_t reserved[8];
};

struct setup_data {
	uint64_t next;
	uint32_t type;
#define SETUP_NONE	0
#define SETUP_E820_EXT	1
#define SETUP_DTB	2
#define SETUP_PCI	3
#define SETUP_EFI	4
#define SETUP_RNG_SEED	9
	uint32_t len;
	uint8_t data[0];
} __attribute__((packed));

static int get_efi_value(const char *filename,
			const char *pattern, uint64_t *val)
{
	FILE *fp;
	char line[1024], *s, *end;

	fp = fopen(filename, "r");
	if (!fp)
		return 1;

	while (fgets(line, sizeof(line), fp) != 0) {
		s = strstr(line, pattern);
		if (!s)
			continue;
		*val = strtoull(s + strlen(pattern), &end, 16);
		if (*val == ULLONG_MAX) {
			fclose(fp);
			return 2;
		}
		break;
	}

	fclose(fp);
	return 0;
}

static int get_efi_values(struct efi_setup_data *esd)
{
	int ret = 0;

	ret = get_efi_value("/sys/firmware/efi/systab", "SMBIOS=0x",
			    &esd->smbios);
	ret |= get_efi_value("/sys/firmware/efi/fw_vendor", "0x",
			     &esd->fw_vendor);
	ret |= get_efi_value("/sys/firmware/efi/runtime", "0x",
			     &esd->runtime);
	ret |= get_efi_value("/sys/firmware/efi/config_table", "0x",
			     &esd->tables);
	return ret;
}

static int get_efi_runtime_map(struct efi_mem_descriptor **map)
{
	DIR *dirp;
	struct dirent *entry;
	char filename[1024];
	struct efi_mem_descriptor md, *p = NULL;
	int nr_maps = 0;

	dirp = opendir("/sys/firmware/efi/runtime-map");
	if (!dirp)
		return 0;
	while ((entry = readdir(dirp)) != NULL) {
		sprintf(filename,
			"/sys/firmware/efi/runtime-map/%s",
			(char *)entry->d_name);
		if (*entry->d_name == '.')
			continue;
		file_scanf(filename, "type", "0x%x", (unsigned int *)&md.type);
		file_scanf(filename, "phys_addr", "0x%llx",
			   (unsigned long long *)&md.phys_addr);
		file_scanf(filename, "virt_addr", "0x%llx",
			   (unsigned long long *)&md.virt_addr);
		file_scanf(filename, "num_pages", "0x%llx",
			   (unsigned long long *)&md.num_pages);
		file_scanf(filename, "attribute", "0x%llx",
			   (unsigned long long *)&md.attribute);
		p = realloc(p, (nr_maps + 1) * sizeof(md));
		if (!p)
			goto err_out;

		*(p + nr_maps) = md;
		*map = p;
		nr_maps++;
	}

	closedir(dirp);
	return nr_maps;
err_out:
	if (*map)
		free(*map);
	closedir(dirp);
	return 0;
}

struct efi_info {
	uint32_t efi_loader_signature;
	uint32_t efi_systab;
	uint32_t efi_memdesc_size;
	uint32_t efi_memdesc_version;
	uint32_t efi_memmap;
	uint32_t efi_memmap_size;
	uint32_t efi_systab_hi;
	uint32_t efi_memmap_hi;
};

/*
 * Add another instance to single linked list of struct setup_data.
 * Please refer to kernel Documentation/x86/boot.txt for more details
 * about setup_data structure.
 */
static void add_setup_data(struct kexec_info *info,
			   struct x86_linux_param_header *real_mode,
			   struct setup_data *sd)
{
	int sdsize = sizeof(struct setup_data) + sd->len;

	sd->next = real_mode->setup_data;
	real_mode->setup_data = add_buffer(info, sd, sdsize, sdsize, getpagesize(),
			    0x100000, ULONG_MAX, INT_MAX);
}

/*
 * setup_efi_data will collect below data and pass them to 2nd kernel.
 * 1) SMBIOS, fw_vendor, runtime, config_table, they are passed via x86
 *    setup_data.
 * 2) runtime memory regions, set the memmap related fields in efi_info.
 */
static int setup_efi_data(struct kexec_info *info,
			  struct x86_linux_param_header *real_mode)
{
	int64_t memmap_paddr;
	struct setup_data *sd;
	struct efi_setup_data *esd;
	struct efi_mem_descriptor *maps;
	int nr_maps, size, ret = 0;
	struct efi_info *ei = (struct efi_info *)real_mode->efi_info;

	ret = access("/sys/firmware/efi/systab", F_OK);
	if (ret < 0)
		goto out;

	esd = malloc(sizeof(struct efi_setup_data));
	if (!esd) {
		ret = 1;
		goto out;
	}
	memset(esd, 0, sizeof(struct efi_setup_data));
	ret = get_efi_values(esd);
	if (ret)
		goto free_esd;
	nr_maps = get_efi_runtime_map(&maps);
	if (!nr_maps) {
		ret = 2;
		goto free_esd;
	}
	sd = malloc(sizeof(struct setup_data) + sizeof(*esd));
	if (!sd) {
		ret = 3;
		goto free_maps;
	}

	memset(sd, 0, sizeof(struct setup_data) + sizeof(*esd));
	sd->next = 0;
	sd->type = SETUP_EFI;
	sd->len = sizeof(*esd);
	memcpy(sd->data, esd, sizeof(*esd));
	free(esd);

	add_setup_data(info, real_mode, sd);

	size = nr_maps * sizeof(struct efi_mem_descriptor);
	memmap_paddr = add_buffer(info, maps, size, size, getpagesize(),
					0x100000, ULONG_MAX, INT_MAX);
	ei->efi_memmap = memmap_paddr & 0xffffffff;
	ei->efi_memmap_hi = memmap_paddr >> 32;
	ei->efi_memmap_size = size;
	ei->efi_memdesc_size = sizeof(struct efi_mem_descriptor);

	return 0;
free_maps:
	free(maps);
free_esd:
	free(esd);
out:
	return ret;
}

static void add_e820_map_from_mr(struct x86_linux_param_header *real_mode,
			struct e820entry *e820, struct memory_range *range, int nr_range)
{
	int i;

	for (i = 0; i < nr_range; i++) {
		e820[i].addr = range[i].start;
		e820[i].size = range[i].end - range[i].start + 1;
		switch (range[i].type) {
			case RANGE_RAM:
				e820[i].type = E820_RAM;
				break;
			case RANGE_ACPI:
				e820[i].type = E820_ACPI;
				break;
			case RANGE_ACPI_NVS:
				e820[i].type = E820_NVS;
				break;
			case RANGE_PMEM:
				e820[i].type = E820_PMEM;
				break;
			case RANGE_PRAM:
				e820[i].type = E820_PRAM;
				break;
			default:
			case RANGE_RESERVED:
				e820[i].type = E820_RESERVED;
				break;
		}
		dbgprintf("%016lx-%016lx (%d)\n",
				e820[i].addr,
				e820[i].addr + e820[i].size - 1,
				e820[i].type);

		if (range[i].type != RANGE_RAM)
			continue;
		if ((range[i].start <= 0x100000) && range[i].end > 0x100000) {
			unsigned long long mem_k = (range[i].end >> 10) - (0x100000 >> 10);
			real_mode->ext_mem_k = mem_k;
			real_mode->alt_mem_k = mem_k;
			if (mem_k > 0xfc00) {
				real_mode->ext_mem_k = 0xfc00; /* 64M */
			}
			if (mem_k > 0xffffffff) {
				real_mode->alt_mem_k = 0xffffffff;
			}
		}
	}
}

static void setup_e820_ext(struct kexec_info *info, struct x86_linux_param_header *real_mode,
			   struct memory_range *range, int nr_range)
{
	struct setup_data *sd;
	struct e820entry *e820;
	int nr_range_ext;

	nr_range_ext = nr_range - E820MAX;
	sd = xmalloc(sizeof(struct setup_data) + nr_range_ext * sizeof(struct e820entry));
	sd->next = 0;
	sd->len = nr_range_ext * sizeof(struct e820entry);
	sd->type = SETUP_E820_EXT;

	e820 = (struct e820entry *) sd->data;
	dbgprintf("Extended E820 via setup_data:\n");
	add_e820_map_from_mr(real_mode, e820, range + E820MAX, nr_range_ext);
	add_setup_data(info, real_mode, sd);
}

static void setup_e820(struct kexec_info *info, struct x86_linux_param_header *real_mode)
{
	struct memory_range *range;
	int nr_range, nr_range_saved;


	if (info->kexec_flags & KEXEC_ON_CRASH && !arch_options.pass_memmap_cmdline) {
		range = info->crash_range;
		nr_range = info->nr_crash_ranges;
	} else {
		range = info->memory_range;
		nr_range = info->memory_ranges;
	}

	nr_range_saved = nr_range;
	if (nr_range > E820MAX) {
		nr_range = E820MAX;
	}

	real_mode->e820_map_nr = nr_range;
	dbgprintf("E820 memmap:\n");
	add_e820_map_from_mr(real_mode, real_mode->e820_map, range, nr_range);

	if (nr_range_saved > E820MAX) {
		dbgprintf("extra E820 memmap are passed via setup_data\n");
		setup_e820_ext(info, real_mode, range, nr_range_saved);
	}
}

static void setup_rng_seed(struct kexec_info *info,
			   struct x86_linux_param_header *real_mode)
{
	struct {
		struct setup_data header;
		uint8_t rng_seed[32];
	} *sd;

	sd = xmalloc(sizeof(*sd));
	sd->header.next = 0;
	sd->header.len = sizeof(sd->rng_seed);
	sd->header.type = SETUP_RNG_SEED;

	if (getrandom(sd->rng_seed, sizeof(sd->rng_seed), GRND_NONBLOCK) !=
	    sizeof(sd->rng_seed))
		return; /* Not initialized, so don't pass a seed. */

	add_setup_data(info, real_mode, &sd->header);
}

static int
get_efi_mem_desc_version(struct x86_linux_param_header *real_mode)
{
	struct efi_info *ei = (struct efi_info *)real_mode->efi_info;

	return ei->efi_memdesc_version;
}

static void setup_efi_info(struct kexec_info *info,
			   struct x86_linux_param_header *real_mode)
{
	int ret, desc_version;
	off_t offset = offsetof(typeof(*real_mode), efi_info);

	ret = get_bootparam(&real_mode->efi_info, offset, 32);
	if (ret)
		return;
	if (((struct efi_info *)real_mode->efi_info)->efi_memmap_size == 0)
		/* zero filled efi_info */
		goto out;
	desc_version = get_efi_mem_desc_version(real_mode);
	if (desc_version != 1) {
		fprintf(stderr,
			"efi memory descriptor version %d is not supported!\n",
			desc_version);
		goto out;
	}
	ret = setup_efi_data(info, real_mode);
	if (ret)
		goto out;

	return;

out:
	memset(&real_mode->efi_info, 0, 32);
	return;
}

void setup_linux_system_parameters(struct kexec_info *info,
				   struct x86_linux_param_header *real_mode)
{
	int err;

	/* get subarch from running kernel */
	setup_subarch(real_mode);
	if (bzImage_support_efi_boot && !arch_options.noefi)
		setup_efi_info(info, real_mode);

	/* Default screen size */
	real_mode->orig_x = 0;
	real_mode->orig_y = 0;
	real_mode->orig_video_page = 0;
	real_mode->orig_video_mode = 0;
	real_mode->orig_video_cols = 80;
	real_mode->orig_video_lines = 25;
	real_mode->orig_video_ega_bx = 0;
	real_mode->orig_video_isVGA = 1;
	real_mode->orig_video_points = 16;

	/* setup vesa fb if possible, or just use original screen_info */
	err = setup_linux_vesafb(real_mode);
	if (err) {
		uint16_t cl_magic, cl_offset;

		/* save and restore the old cmdline param if needed */
		cl_magic = real_mode->cl_magic;
		cl_offset = real_mode->cl_offset;

		err = get_bootparam(real_mode, 0, sizeof(struct screen_info));
		if (!err) {
			real_mode->cl_magic = cl_magic;
			real_mode->cl_offset = cl_offset;
		}
	}
	/* Fill in the memsize later */
	real_mode->ext_mem_k = 0;
	real_mode->alt_mem_k = 0;
	real_mode->e820_map_nr = 0;

	/* Default APM info */
	memset(&real_mode->apm_bios_info, 0, sizeof(real_mode->apm_bios_info));
	/* Default drive info */
	memset(&real_mode->drive_info, 0, sizeof(real_mode->drive_info));
	/* Default sysdesc table */
	real_mode->sys_desc_table.length = 0;

	/* default yes: this can be overridden on the command line */
	real_mode->mount_root_rdonly = 0xFFFF;

	/* default /dev/hda
	 * this can be overrident on the command line if necessary.
	 */
	real_mode->root_dev = (0x3 <<8)| 0;

	/* another safe default */
	real_mode->aux_device_info = 0;

	setup_e820(info, real_mode);

	/* pass RNG seed */
	setup_rng_seed(info, real_mode);

	/* fill the EDD information */
	setup_edd_info(real_mode);

	/* Always try to fill acpi_rsdp_addr */
	real_mode->acpi_rsdp_addr = get_acpi_rsdp();
}

void setup_linux_dtb(struct kexec_info *info, struct x86_linux_param_header *real_mode,
				   const char *dtb_buf, int dtb_len)
{
	struct setup_data *sd;

	sd = xmalloc(sizeof(struct setup_data) + dtb_len);
	sd->next = 0;
	sd->len = dtb_len;
	sd->type = SETUP_DTB;
	memcpy(sd->data, dtb_buf, dtb_len);


	add_setup_data(info, real_mode, sd);
}
