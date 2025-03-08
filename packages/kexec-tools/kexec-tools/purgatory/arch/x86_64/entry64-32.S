/*
 * purgatory:  setup code
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

.data
	.equ	MSR_K6_EFER,   0xC0000080
	.equ	EFER_LME,      0x00000100
	.equ	X86_CR4_PAE,   0x00000020
	.equ	CR0_PG,        0x80000000

	.text
	.balign 16
	.globl entry32, entry32_regs
entry32:
	.code64

	/* Setup the 4G offset of entry32 lm_exit code segment */
	movq	$0x00CF9A000000ffff, %rax

	leaq	entry32(%rip), %rbx	/* Low 24 bits */
	andq	$0xffffff, %rbx
	shlq	$16, %rbx
	orq	%rbx, %rax

	leaq	entry32(%rip), %rbx	/* High 8 bits */
	movq	$0xff000000, %rdx
	andq	%rdx, %rbx
	shlq	$32, %rbx
	orq	%rbx, %rax

	movq	%rax, (gdt + 0x20)(%rip)

	/* Setup a gdt that is generally usefully */
	lgdt	gdt(%rip)

	/* Setup the far pointer to the entry point */
	movl	eip(%rip), %eax
	movl	%eax, entry32_addr(%rip)

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

	/* Load the registers */
	movl	%cs:eax - entry32, %eax
	movl	%cs:ecx - entry32, %ecx
	movl	%cs:edx - entry32, %edx
	movl	%cs:esi - entry32, %esi
	movl	%cs:edi - entry32, %edi
	movl	%cs:esp - entry32, %esp
	movl	%cs:ebp - entry32, %ebp
	movl	%cs:ebx - entry32, %ebx

	/* Jump to the loaded image */
	ljmp	*%cs:entry32_addr - entry32

	.section ".data"
	.balign 16
gdt:	/* 0x00 unusable segment 
	 * 0x08 unused
	 * so use them as the gdt ptr
	 */
	.word	gdt_end - gdt - 1
	.quad	gdt
	.word	0, 0, 0
			
	/* Documented linux kernel segments */
	/* 0x10 4GB flat code segment */
	.word	0xFFFF, 0x0000, 0x9A00, 0x00CF
	/* 0x18 4GB flat data segment */
	.word	0xFFFF, 0x0000, 0x9200, 0x00CF

	/* 0x20 4GB flat code segment base at entry32 */
	.word	0xFFFF, 0x0000, 0x9A00, 0x0CF
	/* 0x28 dummy */
	.word	0x0000, 0x0000, 0x0000, 0x000
	/* 0x30 dummy */
	.word	0x0000, 0x0000, 0x0000, 0x000
	/* 0x38 dummy */
	.word	0x0000, 0x0000, 0x0000, 0x000
	/* 0x40 dummy */
	.word	0x0000, 0x0000, 0x0000, 0x000
	/* 0x48 dummy */
	.word	0x0000, 0x0000, 0x0000, 0x000
	/* 0x50 dummy */
	.word	0x0000, 0x0000, 0x0000, 0x000
	/* 0x58 dummy */
	.word	0x0000, 0x0000, 0x0000, 0x000
	
	/* Segments used by the 2.5.x kernel */
	/* 0x60 4GB flat code segment */
	.word	0xFFFF, 0x0000, 0x9A00, 0x00CF
	/* 0x68 4GB flat data segment */
	.word	0xFFFF, 0x0000, 0x9200, 0x00CF
gdt_end:

	.section ".rodata"
	.balign 4
lm_exit_addr:
	.long lm_exit - entry32
	.long 0x20

	.section ".data"
	.balign 4
entry32_addr:
	.long 0x00000000
	.long 0x10

	.section ".rodata"
	.balign 4
entry32_regs:  
eax:	.long 0x00000000
ebx:	.long 0x00000000
ecx:	.long 0x00000000
edx:	.long 0x00000000
esi:	.long 0x00000000
edi:	.long 0x00000000
esp:	.long 0x00000000
ebp:	.long 0x00000000
eip:	.quad entry16	/* low 32 bits address
			 * high 32bits zeros
			 * uses 64bit reloc
			 */
	.size entry32_regs, (. - 4) - entry32_regs

