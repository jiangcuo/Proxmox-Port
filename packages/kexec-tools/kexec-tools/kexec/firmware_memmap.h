/*
 * firmware_memmap.c: Read /sys/firmware/memmap
 *
 * Created by: Bernhard Walle (bernhard.walle@gmx.de)
 * Copyright (C) SUSE LINUX Products GmbH, 2008. All rights reserved
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
#ifndef FIRMWARE_MEMMAP_H
#define FIRMWARE_MEMMAP_H

#include "kexec.h"

/**
 * Reads the /sys/firmware/memmap interface, documented in
 * Documentation/ABI/testing/sysfs-firmware-memmap (kernel tree).
 *
 * The difference between /proc/iomem and /sys/firmware/memmap is that
 * /sys/firmware/memmap provides the raw memory map, provided by the
 * firmware of the system. That memory map should be passed to a kexec'd
 * kernel because the behaviour should be the same as a normal booted kernel,
 * so any limitation (e.g. by the user providing the mem command line option)
 * should not be passed to the kexec'd kernel.
 *
 * The parsing of the code is independent of the architecture. However, the
 * actual architecture-specific code might postprocess the code a bit, like
 * x86 does.
 */

/**
 * Compares two memory ranges according to their start address. This function
 * can be used with qsort() as @c compar function.
 *
 * @param[in] first a pointer to the first memory range
 * @param[in] second a pointer to the second memory range
 * @return 0 if @p first and @p second have the same start address,
 *         a value less then 0 if the start address of @p first is less than
 *         the start address of @p second, and a value greater than 0 if
 *         the opposite is in case.
 */
int compare_ranges(const void *first, const void *second);

/**
 * Checks if the kernel provides the /sys/firmware/memmap interface.
 * It makes sense to use that function in advance before calling
 * get_firmware_memmap_ranges() because the latter function prints an error
 * if it cannot open the directory. If have_sys_firmware_memmap() returns
 * false, then one can use the old /proc/iomem interface (for older kernels).
 */
int have_sys_firmware_memmap(void);

/**
 * Parses the /sys/firmware/memmap memory map.
 *
 * @param[out] range a pointer to an array of type struct memory_range with
 *             at least *range entries
 * @param[in,out] ranges a pointer to an integer that holds the number of
 *        	  entries which range contains (at least). After successful
 *        	  return, the number of actual entries will be written.
 * @return 0 on success, -1 on failure.
 */
int get_firmware_memmap_ranges(struct memory_range *range, size_t *ranges);


#endif /* FIRMWARE_MEMMAP_H */
