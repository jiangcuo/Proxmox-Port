/*
 * kexec: Linux boots Linux
 *
 * Created by: Mohan Kumar M (mohan@in.ibm.com)
 *
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
#include "../../../kexec/arch/ppc64/crashdump-ppc64.h"

extern unsigned long backup_start;

/* Backup first 32KB of memory to backup region reserved by kexec */
void crashdump_backup_memory(void)
{
	void *dest, *src;

	src = (void *)BACKUP_SRC_START;

	if (backup_start) {
		dest = (void *)(backup_start);
		memcpy(dest, src, BACKUP_SRC_SIZE);
	}
}
