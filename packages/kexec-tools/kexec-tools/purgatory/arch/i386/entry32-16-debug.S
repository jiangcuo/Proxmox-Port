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

#include "arch/debug.h"	

#undef i386	
	.text
	.globl entry16_debug, entry16_debug_regs
	.globl entry16_debug_pre32
	.globl entry16_debug_first32
	.globl entry16_debug_old_first32
	.balign 16
entry16_debug:
	.code32
	.arch i386
	/* Compute where I am running at (assumes esp valid) */
	call	1f
1:	popl	%ebx
	subl	$(1b - entry16_debug), %ebx

	/* Fixup my real mode segment */
	movl	%ebx, %eax
	shrl	$4, %eax
	movw	%ax, (2 + realptr - entry16_debug)(%ebx)

	/* Fixup the gdt */
	leal	(gdt - entry16_debug)(%ebx), %eax
	movl	%eax, (0x02 + gdt - entry16_debug)(%ebx)

	movl	%ebx, %eax
	shll	$16, %eax
	
	movl	%ebx, %ecx
	shrl	$16, %ecx
	andl	$0xff, %ecx
	
	movl	%ebx, %edx
	andl	$0xff000000, %edx
	orl	%edx, %ecx

	orl	%eax, (0x08 + gdt - entry16_debug)(%ebx)
	orl	%ecx, (0x0c + gdt - entry16_debug)(%ebx)
	orl	%eax, (0x10 + gdt - entry16_debug)(%ebx)
	orl	%ecx, (0x14 + gdt - entry16_debug)(%ebx)
	
	
DEBUG_CHAR('a')
	/* Setup the classic BIOS interrupt table at 0x0 */
	lidt	(idtptr - entry16_debug)(%ebx)

DEBUG_CHAR('b')
	/* Provide us with 16bit segments that we can use */
	lgdt	(gdt - entry16_debug)(%ebx)

DEBUG_CHAR('c')
	/* Note we don't disable the a20 line, (this shouldn't be required)
	 * The code to do it is in kexec_test and it is a real pain.
	 * I will worry about that when I need it.
	 */
	
	/* Load 16bit data segments, to ensure the segment limits are set */
	movl	$0x10, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	movl	%eax, %fs
	movl	%eax, %gs

DEBUG_CHAR('d')

	/* switch to 16bit mode */
	ljmp	$0x08, $1f - entry16_debug
1:
	.code16
DEBUG_CHAR('e')
	/* Disable Paging and protected mode */
	/* clear the PG & PE bits of CR0 */
	movl	%cr0,%eax
	andl	$~((1 << 31)|(1<<0)),%eax
	movl	%eax,%cr0

DEBUG_CHAR('f')
	/* make intersegment jmp to flush the processor pipeline
	 * and reload %cs:%eip (to clear upper 16 bits of %eip).
	 */
	ljmp	*(realptr - entry16_debug)
3:
DEBUG_CHAR('g')
	/* we are in real mode now
	 * set up the real mode segment registers : %ds, $ss, %es
	 */
	/* Setup the data segment */
	movw	%cs, %ax
	movw	%ax, %ds

DEBUG_CHAR('h')
	/* Load the registers */
	movl	eax - entry16_debug, %eax
	movl	ebx - entry16_debug, %ebx
	movl	ecx - entry16_debug, %ecx
	movl	edx - entry16_debug, %edx
	movl	esi - entry16_debug, %esi
	movl	edi - entry16_debug, %esi
	movl	esp - entry16_debug, %esp
	movl	ebp - entry16_debug, %ebp
	movw	es  - entry16_debug, %es
	movw	ss  - entry16_debug, %ss
	movw	fs  - entry16_debug, %fs
	movw	gs  - entry16_debug, %gs
	movw	ds  - entry16_debug, %ds

	/* Jump to the kernel entrypoint */
	ljmp	%cs:*(realdest - entry16_debug)

	.balign 4
entry16_debug_regs:	
eax:	.long	0x00000000
ebx:	.long	0x00000000
ecx:	.long	0x00000000
edx:	.long	0x00000000
esi:	.long	0x00000000
edi:	.long	0x00000000
esp:	.long	0x00000000
ebp:	.long	0x00000000
ds:	.word	0x0000
es:	.word	0x0000
ss:	.word	0x0000
fs:	.word	0x0000
gs:	.word	0x0000
realdest:
ip:	.word	0x0000
cs:	.word	0x0000
pad:	.word	0x0000
	.size entry16_debug_regs, . - entry16_debug_regs	

	.balign 16
realptr:		
	.word	3b - entry16_debug
	.word	0x0000
	
	.data
	.balign 16

idtptr:
	/* 256 entry idt at 0 */
	.word	0x400 - 1
	.word	0, 0

gdt:
	/* 0x00 unusable segment so used as the gdt ptr */
	.word gdt_end - gdt - 1
	.long 0 /* gdt */
	.word 0

	/* 0x08 16 bit real mode code segment */
	.word	0xffff, 0x0000
	.byte	0x00, 0x9b, 0x00, 0x00

	/* 0x10 16 bit real mode data segment */
	.word	0xffff, 0x0000
	.byte	0x00, 0x93, 0x00, 0x00
gdt_end:
	.size gdt, . - gdt

	.text	
entry16_debug_pre32:
	.code16
DEBUG_CHAR('i')
	cli					# no interrupts allowed !
	movb	$0x80, %al			# disable NMI for bootup
						# sequence
	outb	%al, $0x70
	lret
.size entry16_debug_pre32, . - entry16_debug_pre32
	
entry16_debug_first32:
	.code32
DEBUG_CHAR('j')
	.byte	0xb8	/* movl $0x10000, %eax */
entry16_debug_old_first32:
	.long 0x100000
	.size entry16_debug_old_first32, . - entry16_debug_old_first32
	jmp	*%eax
.size entry16_debug_first32, . - entry16_debug_first32
