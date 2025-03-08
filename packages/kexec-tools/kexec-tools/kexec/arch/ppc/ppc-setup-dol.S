/*
 * ppc-setup-dol.S - setup glue for Nintendo's GameCube
 * Copyright (C) 2004 Albert Herranz
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#include "ppc_asm.h"

	.data
	.globl setup_dol_start
setup_dol_start:

	/* Try to reproduce the GameCube "native" environment */

        /* Setup BATs */
	isync
	li	r8, 0
	mtspr	DBAT0U, r8
	mtspr	DBAT0L, r8
	mtspr	DBAT1U, r8
	mtspr	DBAT1L, r8
	mtspr	DBAT2U, r8
	mtspr	DBAT2L, r8
	mtspr	DBAT3U, r8
	mtspr	DBAT3L, r8
	mtspr	IBAT0U, r8
	mtspr	IBAT0L, r8
	mtspr	IBAT1U, r8
	mtspr	IBAT1L, r8
	mtspr	IBAT2U, r8
	mtspr	IBAT2L, r8
	mtspr	IBAT3U, r8
	mtspr	IBAT3L, r8

	/*
	 * Memory Map
         * start       end         size  description
	 * 0x80000000  0x817fffff  24MB  RAM, uncached
	 * 0xc0000000  0xc17fffff  24MB  RAM, cached
	 * 0xc8000000  0xc81fffff   2MB  Embedded Framebuffer
	 * 0xcc000000                    Hardware registers
	 * 0xe0000000                    Layer 2 transfer cache ??? 256KB
         *
	 */

	isync
	lis	r8,     0x8000 /* IBAT0,DBAT0 for first 16Mbytes */
	ori	r8, r8, 0x01ff /* 16MB */
	mtspr	IBAT0U, r8
	mtspr	DBAT0U, r8
	li	r8,     0x0002 /* rw */
	mtspr	IBAT0L, r8
	mtspr	DBAT0L, r8

	lis	r8,     0xc000	/* DBAT1 for IO mem */
	ori	r8, r8, 0x1fff  /* 256MB */
	mtspr	DBAT1U, r8
	li	r8,     0x002a  /* uncached, guarded ,rw */
	mtspr	DBAT1L, r8

	lis	r8,     0x8100	/* IBAT2,DBAT2 for next 8Mbytes */
	ori	r8, r8, 0x00ff  /* 8MB */
	mtspr	IBAT2U, r8
	mtspr	DBAT2U, r8
	lis	r8,     0x0100
	ori	r8, r8, 0x0002  /* rw */
	mtspr	IBAT2L, r8
	mtspr	DBAT2L, r8

	lis	r8,     0xe000	/* DBAT3 for layer 2 transfer cache ??? */
	ori	r8, r8, 0x01fe  /* 16MB ??? */
	mtspr	DBAT3U, r8
	lis	r8,     0xe000
	ori	r8, r8, 0x0002  /* rw */
	mtspr	DBAT3L, r8

	sync
	isync

/* AFAIK, this is not strictly needed, although seems sane */
#if 1
	li      r9, 0

	/* page table pointer */
	sync
	mtspr	SDR1, r9

	/* segment registers */
	li	r8, 16
	mtctr	r8
	li	r8, 0
1:	mtsrin	r9, r8		/* zero */
	sync
	addis   r8,r8,0x1000	/* next register */
	bdnz	1b
#endif

	/* switch MMU on and continue */
	RELOC_SYM(1f)
	mfmsr   r0
	ori     r0, r0, MSR_RI|MSR_ME|MSR_DR|MSR_IR
	mtspr   SRR1, r0
	oris    r3, r3, 0x8000		/* adjust text address */
	mtspr   SRR0, r3
	oris	r1, r1, 0x8000		/* adjust stack */
	sync
	rfi

1:
	/* from now on we run in a DOL-like environment */


	/* first, sanitize the hardware a little bit */
	/* although seems to be not needed in the general case */

#if 1
	/* audio */
	lis	r8, 0xcc00	/* io mem */
	li	r9, 0
	sth     r9, 0x5036(r8)	/* stop audio sample */
	stw     r9, 0x6c00(r8)	/* stop streaming */
	stw     r9, 0x6c04(r8)	/* mute */

	/* video */
	mfspr	r8, 920		/* spr920 = HID2 */
	rlwinm	r8, r8, 0, 4, 2 /* stop GX FIFO, and more */
	mtspr	920, r8

	/* exi */
	lis	r8, 0xcc00	/* io mem */
1:      lwz     r9,0x680c(r8)   /* wait for dma transfer to complete */
	andi.   r9,r9,1
	bne+    1b
	stw     r9,0x6800(r8)   /* disable exi interrupts */
	addi    r8,r8,0x14	/* next channel */
	andi.   r9,r8,0x40	/* XXX 4 channels? */
	beq+    1b

	/* pic */
	lis	r8, 0xcc00	/* io mem */
	li	r9, 0
	stw	r9, 0x3004(r8)	/* mask all interrupts */
	stw	r9, 0x3000(r8)	/* clear interrupt cause */

	/* invalidate L1 data and instructions caches */
	mfspr	r8, HID0
	ori	r8, r8, HID0_ICFI|HID0_DCI
	mtspr	HID0, r8
#endif

	/* jump to our entry point */
	RELOC_SYM(setup_dol_regs)
	mr      r9, r3
	lwz     r5, spr8 - setup_dol_regs(r9)

	mtlr    r5
	blr

	.balign 4
	.globl setup_dol_regs
setup_dol_regs:
spr8:	.long 0x00000000

        .balign 4
//#include "isobel_reloc_debug_console.s"

setup_dol_end:

	.globl setup_dol_size
setup_dol_size:	
	.long setup_dol_end - setup_dol_start

