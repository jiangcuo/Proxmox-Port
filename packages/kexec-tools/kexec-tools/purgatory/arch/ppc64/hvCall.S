/*
 * This file contains the generic function to perform a call to the
 * pSeries LPAR hypervisor.
 *
 * Taken from linux/arch/powerpc/platforms/pseries/hvCall.S
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include "ppc64_asm.h"

#define HVSC	.long 0x44000022
.text
	.machine ppc64
.globl DOTSYM(plpar_hcall_norets)
DOTSYM(plpar_hcall_norets):
	or	6,6,6			# medium low priority
        mfcr	0
        stw	0,8(1)

        HVSC 				/* invoke the hypervisor */

        lwz	0,8(1)
        mtcrf	0xff,0
        blr                             /* return r3 = status */
