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

#include "config.h"

	.equ	PROT_CODE_SEG, pmcs - gdt
	.equ	REAL_CODE_SEG, rmcs - gdt
 	.equ	PROT_DATA_SEG, pmds - gdt
 	.equ	REAL_DATA_SEG, rmds - gdt
	.equ	CR0_PE, 1
	/* Gas thinks the .equs for these are non-absolute so use a define */
#define	PROT_CODE_SEG 0x08
#define REAL_CODE_SEG 0x18
#undef i386

	.text
	.arch	i386
	.globl _start
_start:
	.code32
	# Disable interrupts 
	cli
	
	# Save the initial registers 
	movl	%eax, orig_eax
	movl	%ebx, orig_ebx
	movl	%ecx, orig_ecx
	movl	%edx, orig_edx
	movl	%esi, orig_esi
	movl	%edi, orig_edi
	movl	%esp, orig_esp
	movl	%ebp, orig_ebp

	# Setup a stack 
	movl	$stack_end, %esp

	# Display a message to say everything is working so far 
	pushl	$s_hello
	call	print_string
	addl	$4, %esp

	# Save the idt and gdt 
	sidt	orig_idtp
	sgdt	orig_gdtp

	# Display the initial register contents 
	call	print_orig_regs

	pushl	$s_switching_descriptors
	call	print_string
	addl	$4, %esp

	# Load descriptor pointers 
	lgdt	gdtp
	lidt	idtp
	# Reload the data segments
	movl	$PROT_DATA_SEG, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	movl	%eax, %fs
	movl	%eax, %gs

	# Reload %cs 
	ljmp	$PROT_CODE_SEG, $_start.1
_start.1:

	pushl	$s_descriptors_changed
	call	print_string
	addl	$4, %esp

	call	setup_legacy_pic
	pushl	$s_legacy_pic_setup
	call	print_string
	addl	$4, %esp
	
	call	prot_to_real
	.code16

	callw	test16

	/* Return to 32bit mode */
	data32	call	real_to_prot
	.code32
	pushl	$s_in_protected_mode
	call	print_string
	addl	$4, %esp

	pushl	$s_halting
	call	print_string
	addl	$4, %esp
	jmp	halt
	

	/* Go from protected to real mode */
prot_to_real:
	.code32
	/* Load the 16bit idt */
	lidt	idtp_real
	
	popl	%eax
	subl	$RELOC, %eax		/* Adjust return address */
	pushl	%eax
	subl	$RELOC, %esp		/* Adjust stack pointer */
	ljmp	$REAL_CODE_SEG, $1f - RELOC
1:
	.code16
	/* Reload the segment registers to force a 16bit limit */
	movw	$REAL_DATA_SEG, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	%ax, %fs
	movw	%ax, %gs
	
	/* Clear the PE bit of CR0 */
	movl	%cr0, %eax
	andl	$0!CR0_PE, %eax
	movl	%eax, %cr0

	/* make intersegment jmp to flush the processor pipeline
	 * and reload %cs:%eip (to clear upper 16 bits of %eip).
	 */
	data32 ljmp	$(RELOC)>>4,$2f- RELOC
2:
	/* we are in real mode now
	 * set up the real mode segment registers 
	 */
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	movw	%ax,%fs
	movw	%ax,%gs
	data32	ret

real_to_prot:
	.code16
	pushl	%ebx

	/* Compute the address of gdtp */
	movw	%cs, %ax
	shlw	$4, %ax
	movl	$gdtp, %ebx
	subw	%ax, %bx
	
	data32 lgdt %cs:(%bx)
	movl	%cr0, %eax
	orl	$CR0_PE, %eax
	movl	%eax, %cr0

	/* flush prefetch queue and reload %cs:%eip */
	data32 ljmp	$PROT_CODE_SEG, $1f
1:
	.code32
	/* reload other segment registers */
	movl	$PROT_DATA_SEG, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	movl	%eax, %fs
	movl	%eax, %gs

	popl	%ebx		/* Restore %ebx */
	
	addl	$RELOC, %esp	/* Fix up stack pointer */

	popl	%eax		/* Fix up return address */
	addl	$RELOC, %eax
	pushl	%eax

	lidt	idtp		/* Load a dummy idt */
	ret


halt:
	.code32
	hlt
	jmp	halt

print_orig_regs:
	.code32
	# Display the initial register contents 
	pushl	$s_eax
	call	print_string
	pushl	orig_eax
	call	print_hex
	pushl	$space
	call	print_string
	addl	$12, %esp
	
	pushl	$s_ebx
	call	print_string
	pushl	orig_ebx
	call	print_hex
	pushl	$space
	call	print_string
	addl	$12, %esp
	

	pushl	$s_ecx
	call	print_string
	pushl	orig_ecx
	call	print_hex
	pushl	$space
	call	print_string
	addl	$12, %esp
	

	pushl	$s_edx
	call	print_string
	pushl	orig_edx
	call	print_hex
	pushl	$crlf
	call	print_string
	addl	$12, %esp
	

	pushl	$s_esi
	call	print_string
	pushl	orig_esi
	call	print_hex
	pushl	$space
	call	print_string
	addl	$12, %esp
	
	pushl	$s_edi
	call	print_string
	pushl	orig_edi
	call	print_hex
	pushl	$space
	call	print_string
	addl	$12, %esp
	

	pushl	$s_esp
	call	print_string
	pushl	orig_esp
	call	print_hex
	pushl	$space
	call	print_string
	addl	$12, %esp
	

	pushl	$s_ebp
	call	print_string
	pushl	orig_ebp
	call	print_hex
	pushl	$crlf
	call	print_string
	addl	$12, %esp

	# display the interrupt descritor table pointer 
	pushl	$s_idtp
	call	print_string
	movzwl	orig_idtp, %eax
	pushl	%eax
	call	print_hex
	pushl	$space
	call	print_string
	pushl	orig_idt_base
	call	print_hex
	pushl	$crlf
	call	print_string
	addl	$20, %esp

	# display the global descritor table pointer 
	pushl	$s_gdtp
	call	print_string
	movzwl	orig_gdtp, %eax
	pushl	%eax
	call	print_hex
	pushl	$space
	call	print_string
	pushl	orig_gdt_base
	call	print_hex
	pushl	$crlf
	call	print_string
	addl	$20, %esp

	ret


print_string:
	.code32
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	movl	8(%ebp), %esi
	xorl	%eax, %eax
print_string.1:	
	lodsb %ds:(%esi), %al
	testb	$0xff, %al
	jz	print_string.2
	call	print_char
	jmp	print_string.1
print_string.2:
	popl	%esi
	popl	%ebp
	ret


print_hex:
	.code32
	pushl	%ebp
	movl	%esp, %ebp
	movb	$32, %cl
print_hex.1:	
	movl	8(%ebp), %eax
	subb	$4, %cl
	shrl	%cl, %eax
	andb	$0x0f, %al
	cmpb	$9, %al
	ja	print_hex.2
	addb	$'0', %al
	jmp	print_hex.3
print_hex.2:	
	addb	$'A' - 10, %al
print_hex.3:
	pushl	%ecx
	call	print_char
	popl	%ecx
	testb	%cl, %cl
	jnz	print_hex.1

	popl	%ebp
	ret

print_char:
	.code32
	# The character to print is in al 
	call serial_print_char
	retl


#define TTYS0_BASE	0x3f8
#define TTYS0_RBR	(TTYS0_BASE + 0x00)
#define TTYS0_TBR	(TTYS0_BASE + 0x00)
#define TTYS0_LSR	(TTYS0_BASE + 0x05)
serial_print_char:
	.code32
	# The character to print is in al 
	pushl	%eax
	
	# Wait until the serial port is ready to receive characters 
serial_print_char.1:
	movl	$TTYS0_LSR, %edx
	inb	%dx, %al
	testb	$0x20, %al
	jz	serial_print_char.1

	# Output the character 
	movl	$TTYS0_TBR, %edx
	movb	0(%esp), %al
	outb	%al, %dx

	# Wait until the serial port has transmitted the character 
serial_print_char.2:
	movl	$TTYS0_LSR, %edx
	inb	%dx, %al
	testb	$0x40, %al
	jz	serial_print_char.2

	# Restore %eax 
	popl	%eax
	# Return to caller 
	ret

	.code32

idtp_real:
	.word	0x400				# idt limit = 256
	.word	0, 0
idtp:
	.word	0				# idt limit = 0
	.word	0, 0				# idt base = 0L

gdt:
gdtp:
	.word	gdt_end - gdt - 1		# gdt limit
	.long	gdt				# gdt base
	.word	0				# dummy

pmcs:
	# the 32 bit protected mode code segment 
	.word	0xffff,0
	.byte	0,0x9f,0xcf,0

pmds:
	# the 32 bit protected mode data segment 
	.word	0xffff,0
	.byte	0,0x93,0xcf,0

rmcs:
	# the 16 bit real mode code segment 
	.word	0xffff,(RELOC&0xffff)
	.byte	(RELOC>>16),0x9b,0x00,(RELOC>>24)

rmds:
	# the 16 bit real mode data segment 
	.word	0xffff,(RELOC&0xffff)
	.byte	(RELOC>>16),0x93,0x00,(RELOC>>24)
gdt_end:	
	
	
s_hello:
	.ascii	"kexec_test "
	.ascii PACKAGE_VERSION
	.asciz " starting...\r\n"
s_switching_descriptors:
	.asciz	"Switching descriptors.\r\n"
s_descriptors_changed:
	.asciz	"Descriptors changed.\r\n"
s_legacy_pic_setup:
	.asciz	"Legacy pic setup.\r\n"
s_in_protected_mode:
	.asciz	"In protected mode.\r\n"
s_halting:
	.asciz	"Halting.\r\n"
	

space:	.asciz	" "
crlf:	.asciz	"\r\n"
s_eax:	.asciz	"eax: "
s_ebx:	.asciz	"ebx: "
s_ecx:	.asciz	"ecx: "
s_edx:	.asciz	"edx: "
s_esi:	.asciz	"esi: "
s_edi:	.asciz	"edi: "
s_esp:	.asciz	"esp: "
s_ebp:	.asciz	"ebp: "
	

s_idtp:	.asciz	"idt: "
s_gdtp:	.asciz	"gdt: "

#include "x86-setup-legacy-pic.S"

	.bss
	.balign 4096
stack:
	.skip 4096
stack_end:	
	
	.bss
	.balign 4	
orig_eax:	.long 0
orig_ebx:	.long 0
orig_ecx:	.long 0
orig_edx:	.long 0
orig_esi:	.long 0
orig_edi:	.long 0
orig_esp:	.long 0
orig_ebp:	.long 0

	.balign 4
orig_idtp:	.short 0
orig_idt_base:	.long  0
orig_gdtp:	.short 0
orig_gdt_base:	.long  0

