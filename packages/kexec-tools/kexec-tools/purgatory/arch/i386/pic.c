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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <sys/io.h>
#include <purgatory.h>
#include "purgatory-x86.h"


void x86_setup_legacy_pic(void)
{
	/* Load the legacy dos settings into the 8259A pic */
	outb(0xff, 0x21);	/* mask all of 8259A-1 */
	outb(0xff, 0xa1);	/* mask all of 8259A-2 */

	outb(0x11, 0x20);	/* ICW1: select 8259A-1 init */
	outb(0x11, 0x80);	/* A short delay */

	outb(0x08, 0x21);	/* ICW2: 8259A-1 IR0-7 mappend to 0x8-0xf */
	outb(0x08, 0x80);	/* A short delay */

	outb(0x01, 0x21);	/* Normal 8086 auto EOI mode */
	outb(0x01, 0x80);	/* A short delay */

	outb(0x11, 0xa0);	/* ICW1: select 8259A-2 init */
	outb(0x11, 0x80);	/* A short delay */

	outb(0x70, 0xa1);	/* ICW2: 8259A-2 IR0-7 mappend to 0x70-0x77 */
	outb(0x70, 0x80);	/* A short delay */

	outb(0x01, 0xa1);	/* Normal 8086 auto EOI mode */
	outb(0x01, 0x80);	/* A short delay */

	outb(0x00, 0x21);	/* Unmask all of 8259A-1 */
	outb(0x00, 0xa1);	/* Unmask all of 8259A-2 */
}

