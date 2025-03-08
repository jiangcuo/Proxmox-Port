#
#  kexec: Linux boots Linux
#
#  Copyright (C) 2004 - 2005, Milton D Miller II, IBM Corporation
#  Copyright (C) 2006, Mohan Kumar M (mohan@in.ibm.com), IBM Corporation
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation (version 2 of the License).
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

#include "ppc64_asm.h"

# v2wrap.S
# a wrapper to call purgatory code to backup first
# 32kB of first kernel into the backup region
# reserved by kexec-tools.
# Invokes ppc64 kernel with the expected arguments
# of kernel(device-tree, phys-offset, 0)

#
# calling convention:
#   r3 = physical number of this cpu (all cpus)
#   r4 = address of this chunk (master only)
# master enters at purgatory_start (aka first byte of this chunk)
# slaves (additional cpus), if any, enter a copy of the
# first 0x100 bytes of this code relocated to 0x0
#
# in other words,
#   a copy of the first 0x100 bytes of this code is copied to 0
#   and the slaves are sent to address 0x60
#   with r3 = their physical cpu number.

#define LOADADDR(rn,name) \
	lis     rn,name##@highest;      \
	ori     rn,rn,name##@higher;    \
	rldicr  rn,rn,32,31;            \
	oris    rn,rn,name##@h;         \
	ori     rn,rn,name##@l

	.machine ppc64
	.align 8
	.globl purgatory_start
purgatory_start:	b	master
	.org purgatory_start + 0x5c     # ABI: possible run_at_load flag at 0x5c
	.globl run_at_load
run_at_load:
	.long 0
	.size run_at_load, . - run_at_load
	.org purgatory_start + 0x60     # ABI: slaves start at 60 with r3=phys
slave:	b $
	.org purgatory_start + 0x100    # ABI: end of copied region
	.size purgatory_start, . - purgatory_start

#
# The above 0x100 bytes at purgatory_start are replaced with the
# code from the kernel (or next stage) by kexec/arch/ppc64/kexec-elf-ppc64.c
#

master:
	or	1,1,1		# low priority to let other threads catchup
	isync
	mr      17,3            # save cpu id to r17
	mr      15,4            # save physical address in reg15

	LOADADDR(6,my_toc)
	ld      2,0(6)          #setup toc

	LOADADDR(6,stack)
	ld      1,0(6)          #setup stack

	subi    1,1,112
	bl      DOTSYM(purgatory)
	nop

	or	3,3,3		# ok now to high priority, lets boot
	lis	6,0x1
	mtctr	6		# delay a bit for slaves to catch up
83:	bdnz	83b		# before we overwrite 0-100 again

	LOADADDR(16, dt_offset)
	ld      3,0(16)         # load device-tree address
	mr      16,3            # save dt address in reg16
#ifdef __BIG_ENDIAN__
	lwz     6,20(3)         # fetch version number
#else
	li	4,20
	lwbrx	6,3,4		# fetch BE version number
#endif
	cmpwi   0,6,2           # v2 ?
	blt     80f
#ifdef __BIG_ENDIAN__
	stw     17,28(3)        # save my cpu number as boot_cpu_phys
#else
	li	4,28
	stwbrx	17,3,4		# Store my cpu as BE value
#endif
80:
	LOADADDR(6,opal_base)	# For OPAL early debug
	ld      8,0(6)          # load the OPAL base address in r8
	LOADADDR(6,opal_entry)	# For OPAL early debug
	ld      9,0(6)          # load the OPAL entry address in r9
	LOADADDR(6,kernel)
	ld      4,0(6)          # load the kernel address
	LOADADDR(6,run_at_load) # the load flag
	lwz	7,0(6)		# possibly patched by kexec-elf-ppc64
	stw	7,0x5c(4)	# and patch it into the kernel
	mr      3,16            # restore dt address

	mfmsr	5
	andi.	10,5,1		# test MSR_LE
	bne	little_endian

	li	5,0		# r5 will be 0 for kernel
	mtctr	4		# prepare branch to
	bctr			# start kernel
	
little_endian:			# book3s-only
	mtsrr0	4		# prepare branch to

	clrrdi	5,5,1		# clear MSR_LE
	mtsrr1	5

	li	5,0		# r5 will be 0 for kernel

				# skip cache flush, do we care?

	rfid			# update MSR and start kernel
