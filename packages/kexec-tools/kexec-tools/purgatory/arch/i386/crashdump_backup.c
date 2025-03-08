/*
 * kexec: Linux boots Linux
 *
 * Created by:  Vivek goyal (vgoyal@in.ibm.com)
 * Copyright (C) IBM Corporation, 2005. All rights reserved
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

#include <stdint.h>
#include <string.h>

/* Backup region start gets set after /proc/iomem has been parsed. */
/* We reuse the same code for x86_64 also so changing backup_start to
   unsigned long */
unsigned long  backup_start = 0;

unsigned long backup_src_start = 0;
unsigned long backup_src_size = 0;

/* Backup first 640K of memory to backup region as reserved by kexec.
 * Assuming first 640K has to be present on i386 machines and no address
 * validity checks have to be performed. */

void crashdump_backup_memory(void)
{
	void *dest, *src;
	size_t size;

	src = (void *) backup_src_start;
	size = (size_t) backup_src_size;

	if (!size)
		return;

	if (backup_start) {
		dest = (void *)(backup_start);
		memcpy(dest, src, size);
	}
}
