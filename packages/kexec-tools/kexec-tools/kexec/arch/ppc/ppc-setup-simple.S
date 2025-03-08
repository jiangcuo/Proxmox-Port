/*
 * ppc-setup-simple.S - (hopefully) setup for simple embedded platforms
 * Copyright (C) 2004 Albert Herranz
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

/*
 * Only suitable for platforms booting with MMU turned off.
 * -- Albert Herranz
 */

#include "ppc_asm.h"

	.data
	.globl setup_simple_start
setup_simple_start:

	/* should perform here any required setup */

	RELOC_SYM(setup_simple_regs)
	mr      r9, r3
	lwz     r5, spr8 - setup_simple_regs(r9)

	mtlr    r5
	blr

	.balign 4
	.globl setup_simple_regs
setup_simple_regs:
spr8:	.long 0x00000000

setup_simple_end:

	.globl setup_simple_size
setup_simple_size:	
	.long setup_simple_end - setup_simple_start

