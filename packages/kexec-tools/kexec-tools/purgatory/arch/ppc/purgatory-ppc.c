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

#include <purgatory.h>
#include "purgatory-ppc.h"

unsigned int panic_kernel = 0;
unsigned long backup_start = 0;
unsigned long stack = 0;
unsigned long dt_offset = 0;
unsigned long my_thread_ptr = 0;
unsigned long kernel = 0;

void setup_arch(void)
{
	return;
}

void post_verification_setup_arch(void)
{
#ifndef CONFIG_BOOKE
	if (panic_kernel)
		crashdump_backup_memory();
#endif
}

void crashdump_backup_memory(void)
{
	return;
}
