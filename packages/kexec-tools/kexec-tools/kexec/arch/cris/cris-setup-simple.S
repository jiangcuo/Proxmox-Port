/*
 * cris-setup-simple.S - code to execute before stepping into the new kernel.
 * Copyright (C) 2008 AXIS Communications AB
 * Written by Edgar E. Iglesias
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

	.data
	.globl cris_trampoline
cris_trampoline:
	.balign 4
	lapc	cris_regframe, $sp
	moveq	0, $r0
	move	$r0, $pid

	movem	[$sp+], $r14
	jump	$r0
	nop

	.globl cris_regframe
cris_regframe:
	.balign	4
	.fill	16, 4, 0
cris_trampoline_end:

	.globl cris_trampoline_size
cris_trampoline_size:
	.long cris_trampoline_end - cris_trampoline

