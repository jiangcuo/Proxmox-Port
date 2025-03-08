/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2003,2004,2005  Eric Biederman (ebiederm@xmission.com)
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

	.equ	MSR_K6_EFER,   0xC0000080
	.equ	EFER_LME,      0x00000100
	.equ	X86_CR4_PAE,   0x00000020
	.equ	CR0_PG,        0x80000000

	.globl compat_x86_64, compat_x86_64_entry32
	.text
	.code64
	.balign 16	
compat_x86_64:
	/* Setup a temporary gdt */
	/* This also acts as a serializing instruction ensuring
	 * my self modifying code works.
	 */
	lgdt	gdt(%rip)

	/* Switch to 32bit compatiblity mode */
	ljmp	*lm_exit_addr(%rip)
lm_exit:
	.code32

	/* Disable paging */
	movl	%cr0, %eax
	andl	$~CR0_PG, %eax
	movl	%eax, %cr0

	/* Disable long mode */
	movl	$MSR_K6_EFER, %ecx
	rdmsr
	andl	$~EFER_LME, %eax
	wrmsr

	/* Disable PAE */
	xorl	%eax, %eax
	movl	%eax, %cr4

	/* load the data segments */
	movl	$0x18, %eax	/* data segment */
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	movl	%eax, %fs
	movl	%eax, %gs

	/* set all of the registers to known values */
	/* leave %esp alone */

	xorl	%eax, %eax
	xorl	%ebx, %ebx
	xorl    %ecx, %ecx
	xorl    %edx, %edx
	xorl    %esi, %esi
	xorl    %edi, %edi
	xorl    %ebp, %ebp

	jmp	*compat_x86_64_entry32

	.section ".rodata"
	.balign 16
gdt:	/* 0x00 unusable segment 
	 * 0x08 unused
	 * so use them as the gdt ptr
	 */
	.word	gdt_end - gdt - 1
	# A quad word pointer to the gdt with the high 32bits 0
	.long	gdt, 0  
	.word	0, 0, 0

	/* 0x10 4GB flat code segment */
	.word	0xFFFF, 0x0000, 0x9A00, 0x00CF
	/* 0x18 4GB flat data segment */
	.word	0xFFFF, 0x0000, 0x9200, 0x00CF
gdt_end:

lm_exit_addr:
	.long	lm_exit
	.long	0x10	

compat_x86_64_entry32:
	.long 0
.size compat_x86_64_entry32, . - compat_x86_64_entry32	
