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


#undef i386

	.text
	.globl purgatory_start
purgatory_start:
	.code32
	.arch	i386

	/* Load a gdt so I know what the segment registers are */
	lgdt	%cs:gdt

	/* load the data segments */
	movl	$0x08, %eax	/* data segment */
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	movl	%eax, %fs
	movl	%eax, %gs

	/* load the code segment */
	ljmp	$0x10,$1f
1:		

	movl	0(%esp), %eax
	movl	%eax, jump_back_entry

	/* Setup a stack */
	movl	$lstack_end, %esp

	/* Call the C code */
	call	purgatory
	jmp	entry32

	.section ".rodata"
	.balign 16
gdt:
	/* 0x00 unusable segment so used as the gdt ptr */
	.word	gdt_end - gdt - 1
	.long	gdt
	.word	0

	/* 0x8 4GB flat data segment */
	.word	0xFFFF, 0x0000, 0x9200, 0x00CF

	/* 0x10 4GB flat code segment */
	.word	0xFFFF, 0x0000, 0x9A00, 0x00CF
gdt_end:

	/* A stack for the purgatory code */       
	.bss
	.balign 4096
lstack:	
	.skip 4096
lstack_end:

