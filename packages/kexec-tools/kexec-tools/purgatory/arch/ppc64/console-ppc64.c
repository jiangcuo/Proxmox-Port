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
#include "hvCall.h"
#include <byteswap.h>
#include <endian.h>
#include <asm/byteorder.h>

extern int debug;

void putchar(int c)
{
	char buff[16] = "";
	unsigned long *lbuf = (unsigned long *)buff;

	if (!debug) /* running on non pseries */
		return;

	if (c == '\n')
		putchar('\r');

	buff[0] = c;
	plpar_hcall_norets(H_PUT_TERM_CHAR, 0, 1,
			   __cpu_to_be64(lbuf[0]), __cpu_to_be64(lbuf[1]));
	return;
}
