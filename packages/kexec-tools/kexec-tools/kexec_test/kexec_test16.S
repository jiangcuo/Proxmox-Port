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

	.text
	.code16

	.globl test16
	.balign 16
	.globl _start16
_start16:
test16:
	pushw	$s_in_real_mode - _start16
	call	print_string16
	addw	$2, %sp

#if 0
	/* Disable interrupts */
	movb	$0xff, %al
	outb	%al, $0x21
	outb	%al, $0xa1
#endif
	/* Enable interrupts, BIOS calls may fail if we don't */
	sti
	pushw	$s_interrupts_enabled - _start16
	call	print_string16
	addw	$2, %sp

	/* Get the base memory size, via a bios call */
	/* This is to test BIOS calls more than to achieve anything practical */
	xorw	%ax, %ax
	int	$0x12
	pushw	%ax
	pushw	$s_base_memory_size - _start16
	call	print_string16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp
	pushw	$s_crlf - _start16
	call	print_string16
	addw	$2, %sp


	/* Some things do not like a20 being enabled so disable it */
	call	disable_a20

	/* Here we test various BIOS calls to determine how much of the system is working */
	call	get_meme820
	call	print_meme820
	call	print_meme801
	call	print_mem88
	call	disable_apm
	call	print_equipment_list
	call	print_sysdesc
	call	print_video
	call	print_cursor
	call	print_video_mode
	call	set_auto_repeat_rate
	call	print_dasd_type
	call	print_edd
	
	/* Enable a20 */
	call	enable_a20
	pushw	$s_a20_enabled - _start16
	call	print_string16
	addw	$2, %sp

	/* Disable interrupts */
	cli
	pushw	$s_interrupts_disabled - _start16
	call	print_string16
	addw	$2, %sp

	retw

#
# Enable A20.  This is at the very best an annoying procedure.
# A20 code ported from SYSLINUX 1.52-1.63 by H. Peter Anvin.
#

A20_TEST_LOOPS		=  32		# Iterations per wait
A20_ENABLE_LOOPS	= 255		# Total loops to try		
A20_DISABLE_LOOPS	= 255		# Total loops to try

enable_a20:
	.code16
	movb	$A20_ENABLE_LOOPS, a20_tries - _start16
a20_try_loop:

	# First, see if we are on a system with no A20 gate.
a20_none:
	call	a20_test
	jnz	a20_done

	# Next, try the BIOS (INT 0x15, AX=0x2401)
a20_bios:
	movw	$0x2401, %ax
	pushfl					# Be paranoid about flags
	int	$0x15
	popfl

	call	a20_test
	jnz	a20_done

	# Try enabling A20 through the keyboard controller
a20_kbc:
	call	empty_8042

	call	a20_test			# Just in case the BIOS worked
	jnz	a20_done			# but had a delayed reaction.

	movb	$0xD1, %al			# command write
	outb	%al, $0x64
	call	empty_8042

	movb	$0xDF, %al			# A20 on
	outb	%al, $0x60
	call	empty_8042

	# Wait until a20 really *is* enabled; it can take a fair amount of
	# time on certain systems; Toshiba Tecras are known to have this
	# problem.
a20_kbc_wait:
	xorw	%cx, %cx
a20_kbc_wait_loop:
	call	a20_test
	jnz	a20_done
	loop	a20_kbc_wait_loop

	# Final attempt: use "configuration port A"
a20_fast:
	inb	$0x92, %al			# Configuration Port A
	orb	$0x02, %al			# "fast A20" version
	andb	$0xFE, %al			# dont accidentally reset
	outb	%al, $0x92

	# Wait for configuration port A to take effect
a20_fast_wait:
	xorw	%cx, %cx
a20_fast_wait_loop:
	call	a20_test
	jnz	a20_done
	loop	a20_fast_wait_loop

	# A20 is still not responding.  Try frobbing it again.
	# 
	decb	(a20_tries - _start16)
	jnz	a20_try_loop
	jmp	a20_die

a20_die:
	pushw	$s_a20_err_msg - _start16
	call	print_string16
	jmp	halt16

	# If we get here, all is good
a20_done:
	ret
	


# This routine tests whether or not A20 is enabled.  If so, it
# exits with zf = 0.
#
# The memory address used, 0x200, is the int $0x80 vector, which
# should be safe.

A20_TEST_ADDR = 4*0x80

a20_test:
	.code16
	pushw	%cx
	pushw	%ax
	xorw	%cx, %cx
	movw	%cx, %fs			# Low memory
	decw	%cx
	movw	%cx, %gs			# High memory area
	movw	$A20_TEST_LOOPS, %cx
	movw	%fs:(A20_TEST_ADDR), %ax
	pushw	%ax
a20_test_wait:
	incw	%ax
	movw	%ax, %fs:(A20_TEST_ADDR)
	call	delay				# Serialize and make delay constant
	cmpw	%gs:(A20_TEST_ADDR+0x10), %ax
	loope	a20_test_wait

	popw	%fs:(A20_TEST_ADDR)
	popw	%ax
	popw	%cx

	ret

#
# Disable A20
#

disable_a20:
	.code16
	movb	$A20_DISABLE_LOOPS, a20_disable_tries - _start16
a20_disable_loop:

	# First see if gate A20 is already disabled
	call	a20_test
	jz	a20_disabled


	# Next, try the BIOS (INT 0x15, AX= 0x2400)
	movw	$0x2400, %ax
	pushfl					# Be paranoid about flags
	int	$0x15
	popfl

	call	a20_test
	jz	a20_disabled

	# Try disabling A20 through the keyboard controller
	call	empty_8042

	call	a20_test		# Just in case the BIOS worked 
	jz	a20_disabled		# but had a delayed reaction.

	movb	$0xD1, %al		# command write
	outb	%al, $0x64
	call	empty_8042

	movb	$0xDD, %al		# A20 off
	outb	%al, $0x60
	call	empty_8042

	# Wait until a20 really *is* disabled
	xorw	%cx, %cx
a20_kbc_disable_loop:
	call	a20_test
	jz	a20_disabled
	loop	a20_kbc_disable_loop

	# Final attempt: use "configuration port A"
	inb	$0x92, %al		# Configuratin Port A
	andb	$0xFD, %al		# "fast A20" version
	andb	$0xFE, %al		# dont accidentally reset
	outb	%al, $0x92

	# Wait for configuration port A to take affect
	xorw	%cx, %cx
a20_fast_disable_loop:	
	call	a20_test
	jz	a20_disabled
	loop	a20_fast_disable_loop

	# A20 is still not responding.  Try it again
	decb	(a20_disable_tries - _start16)
	jnz	a20_disable_loop

	pushw	$s_a20_cant_disable - _start16
	call	print_string16
	addw	$2, %sp
	retw
	
	# If we get here, all is good
a20_disabled:
	pushw	$s_a20_disabled - _start16
	call	print_string16
	addw	$2, %sp
	retw


# This routine checks that the keyboard command queue is empty
# (after emptying the output buffers)
#
# Some machines have delusions that the keyboard buffer is always full
# with no keyboard attached...
#
# If there is no keyboard controller, we will usually get 0xff
# to all the reads.  With each IO taking a microsecond and
# a timeout of 100,000 iterations, this can take about half a
# second ("delay" == outb to port 0x80). That should be ok,
# and should also be plenty of time for a real keyboard controller
# to empty.
#

empty_8042:
	.code16
	pushl	%ecx
	movl	$100000, %ecx

empty_8042_loop:
	decl	%ecx
	jz	empty_8042_end_loop

	call	delay

	inb	$0x64, %al			# 8042 status port
	testb	$1, %al				# output buffer?
	jz	no_output

	call	delay
	inb	$0x60, %al			# read it
	jmp	empty_8042_loop

no_output:
	testb	$2, %al				# is input buffer full?
	jnz	empty_8042_loop			# yes - loop
empty_8042_end_loop:
	popl	%ecx
	ret




# method E820H:
# the memory map from hell.  e820h returns memory classified into
# a whole bunch of different types, and allows memory holes and
# everything.  We scan through this memory map and build a list
# of the first 32 memory areas, which we return at [E820MAP].
# This is documented at http://www.teleport.com/~acpi/acpihtml/topic245.htm

#define SMAP  0x534d4150
#define E820_MAX 32
#define E820_SIZE 20

get_meme820:
	.code16
	pushw	%bp
	movw	%sp, %bp
	pushw	%ds
	pushw	%es
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	
	xorl	%eax, %eax
	movb	%al, e820nr - _start16
	xorl	%ebx, %ebx			# continuation counter
	movw	$e820_map - _start16, %di		# point into the whitelist
						# so we can have the bios
						# directly write into it.

jmpe820:
	movl	$0x0000e820, %eax		# e820, upper word zeroed
	movl	$SMAP, %edx			# ascii SMAP
	movl	$E820_SIZE, %ecx		# size of the e820rec
	pushw	%ds				# data record.
	popw	%es
	int	$0x15				# make the call
	jc	bail820				# fall to e801 if it fails

	cmpl	$SMAP, %eax			# check the return is SMAP
	jne	bail820				# fall to e801 if it fails

#	cmpl	$1, 16(%di)			# is this usable memory?
#	jne	again820

	# If this is usable memory, we save it by simply advancing %di by
	# sizeof(e820rec).
	#
good820:
	movb	e820nr - _start16, %al		# up to 32 entries
	cmpb	$E820_MAX, %al
	jnl	bail820

	incb	e820nr - _start16
	movw	%di, %ax
	addw	$20, %ax
	movw	%ax, %di
again820:
	cmpl	$0, %ebx			# check to see if
	jne	jmpe820				# %ebx is set to EOF
bail820:
	popl	%ebx
	popl	%edi
	popl	%esi
	popw	%es
	popw	%ds
	popw	%bp
	retw


print_meme820:
	.code16
	pushw	%si
	xorw	%cx, %cx
	movb	(e820nr - _start16), %cl
	movw	$e820_map - _start16, %si

	pushw	$s_meme820 - _start16
	call	print_string16
	addw	$2, %sp
	
print_meme820.1:
	pushw	%cx
	
	pushw	8(%si)
	pushw	10(%si)
	pushw	12(%si)
	pushw	14(%si)
	call	print_hex16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp

	pushw	$s_at - _start16
	call	print_string16
	addw	$2, %sp
	
	pushw	0(%si)
	pushw	2(%si)
	pushw	4(%si)
	pushw	6(%si)
	call	print_hex16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp

	pushw	$s_type - _start16
	call	print_string16
	addw	$2, %sp

	pushw	16(%si)
	pushw	18(%si)
	call	print_hex16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp

	pushw	$s_crlf - _start16
	call	print_string16
	addw	$2, %sp

	popw	%cx
	addw	$E820_SIZE, %si
	subw	$1, %cx
	jnz	print_meme820.1
	
	popw	%si
	retw



print_meme801:
	.code16
	pushw	%bp
	movw	%sp, %bp
	pushw	%bx
	pushl	$0

# method E801H:
# memory size is in 1k chunksizes

	stc					# fix to work around buggy
	xorw	%cx,%cx				# BIOSes which dont clear/set
	xorw	%dx,%dx				# carry on pass/error of
						# e801h memory size call
						# or merely pass cx,dx though
						# without changing them.
	movw	$0xe801, %ax
	int	$0x15
	jc	print_meme801.2

	cmpw	$0x0, %cx			# Kludge to handle BIOSes
	jne	e801usecxdx			# which report their extended
	cmpw	$0x0, %dx			# memory in AX/BX rather than
	jne	e801usecxdx			# CX/DX.  The spec I have read
	movw	%ax, %cx			# seems to indicate AX/BX 
	movw	%bx, %dx			# are more reasonable anyway...

e801usecxdx:
	andl	$0xffff, %edx			# clear sign extend
	shll	$6, %edx			# and go from 64k to 1k chunks
	movl	%edx, -6(%bp)			# store extended memory size
	andl	$0xffff, %ecx			# clear sign extend
 	addl	%ecx, -6(%bp)			# and add lower memory into

	pushw	$s_meme801 - _start16
	call	print_string16
	addw	$2, %sp

	pushw	-6(%bp)
	pushw	-4(%bp)
	call	print_hex16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp
	
	pushw	$s_crlf - _start16
	call	print_string16
	addw	$2, %sp

print_meme801.2:
	addw	$4, %sp
	popw	%bx
	popw	%bp
	retw

print_mem88:
	.code16
# Ye Olde Traditional Methode.  Returns the memory size (up to 16mb or
# 64mb, depending on the bios) in ax.
	movb	$0x88, %ah
	int	$0x15
	
	pushw	%ax
	pushw	$s_mem88 - _start16
	call	print_string16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp
	pushw	$s_crlf - _start16
	call	print_string16
	addw	$2, %sp
	
	retw

print_dasd_type:
	.code16
	pushw	$s_dasd_type - _start16
	call	print_string16
	addw	$2, %sp

	movw	$0x1500, %ax
	movb	$0x81, %dl
	int	$0x13
	jc	print_dasd_type.1

	pushw	%dx
	pushw	%cx
	pushw	$s_space - _start16
	pushw	%ax

	call	print_hex16
	addw	$2, %sp
	call	print_string16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp
	jmp	print_dasd_type.2
print_dasd_type.1:
	pushw	$s_none - _start16
	call	print_string16
	addw	$2, %sp
	
print_dasd_type.2:
	pushw	$s_crlf - _start16
	call	print_string16
	addw	$2, %sp

	retw

print_equipment_list:
	.code16
	pushw	$s_equipment_list - _start16
	call	print_string16
	addw	$2, %sp
	
	int	$0x11
	pushw	%ax
	call	print_hex16
	addw	$2, %sp

	pushw	$s_crlf - _start16
	call	print_string16
	addw	$2, %sp

	retw

print_sysdesc:
	.code16
	pushw	$s_sysdesc - _start16
	call	print_string16
	addw	$2, %sp

	pushw	%es
	movb	$0xc0, %ah
	stc
	int	$0x15
	movw	%es, %ax
	popw	%es
	jc	print_sysdesc.1

	pushw	%bx
	pushw	$s_colon - _start16
	pushw	%ax
	call	print_hex16
	addw	$2, %sp
	call	print_string16
	addw	$2, %sp
	call	print_hex16
	addw	$2, %sp
	jmp	print_sysdesc.2
	
print_sysdesc.1:
	pushw	$s_none - _start16
	call	print_string16
	addw	$2, %sp
	
print_sysdesc.2:
	pushw	$s_crlf - _start16
	call	print_string16
	addw	$2, %sp

	retw

print_edd:
	.code16
	pushw	$s_edd - _start16
	call	print_string16
	add	$2, %sp

	movb	$0x80, %dl
	movb	$0x41, %ah			# Function 41
	movw	$0x55aa, %bx			# magic
	int	$0x13				# make the call
	jc	print_edd.1			# no more BIOS devices
	
    	cmpw	$0xAA55, %bx			# is magic right?
	jne	print_edd.1			# nope

	pushw	$s_ok - _start16
	call	print_string16
	add	$2, %sp
	jmp	print_edd.2

print_edd.1:
	pushw	$s_none - _start16
	call	print_string16
	add	$2, %sp
	
print_edd.2:
	pushw	$s_crlf - _start16
	call	print_string16
	addw	$2, %sp

	retw

set_auto_repeat_rate:
	.code16
	pushw	$s_auto_repeat_rate - _start16
	call	print_string16
	add	$2, %sp
	
# Set the keyboard repeat rate to the max
	movw	$0x0305, %ax
	xorw	%bx, %bx
	int	$0x16

	pushw	$s_done - _start16
	call	print_string16
	add	$2, %sp

	retw

print_video:
	.code16
	pushw	$s_video_type - _start16
	call	print_string16
	add	$2, %sp
	
	movb	$0x12, %ah			# Check EGA/VGA
	movb	$0x10, %bl
	int	$0x10
	movw	$s_video_pre_ega - _start16, %cx
	cmpb	$0x10, %bl
	je	print_video.1

	movw	$0x1a00, %ax			# Check EGA or VGA?
	int	$0x10
	movw	$s_video_vga - _start16, %cx
	cmpb	$0x1a, %al			# 1a means VGA...
	je	print_video.1			# anything else is EGA.

	movw	$s_video_ega - _start16, %cx

print_video.1:
	pushw	%cx
	call	print_string16
	addw	$2, %sp

	pushw	$s_crlf - _start16
	call	print_string16
	addw	$2, %sp
	
	retw

print_cursor:
	.code16
	pushw	$s_cursor - _start16
	call	print_string16
	add	$2, %sp

	movb	$0x03, %ah			# Read cursor position
	xorb	%bh, %bh
	int	$0x10

	xorw	%ax, %ax
	movb	%dl, %al
	pushw	%ax
	pushw	$s_space - _start16
	movb	%dh, %al
	pushw	%ax

	call	print_hex16
	add	$2, %sp
	call	print_string16
	add	$2, %sp
	call	print_hex16
	add	$2, %sp

	pushw	$s_crlf - _start16
	call	print_string16
	add	$2, %sp
	
	retw

print_video_mode:
	.code16
	pushw	$s_video_mode - _start16
	call	print_string16
	add	$2, %sp

	movb	$0x0f, %ah			# Read cursor position
	int	$0x10

	xorb	%ah, %ah
	pushw	%ax
	call	print_hex16
	add	$2, %sp

	pushw	$s_crlf - _start16
	call	print_string16
	add	$2, %sp
	
	retw


disable_apm:
	push	%bp
	movw	%sp, %bp
	pushw	%bx

	pushw	$s_testing_for_apm - _start16
	call	print_string16
	add	$2, %sp
	
	# check for APM BIOS
	movw    $0x5300, %ax    # APM BIOS installation check
	xorw    %bx, %bx
	int     $0x15
	jc      done_apm_bios   # error -> no APM BIOS
	
	cmpw    $0x504d, %bx    # check for "PM" signature
	jne     done_apm_bios   # no signature -> no APM BIOS
	
	pushw	$s_apm_found_disconnecting - _start16
	call	print_string16
	add	$2, %sp
	
	movw    $0x5304, %ax    # Disconnect first just in case
	xorw    %bx, %bx
	int     $0x15           # ignore return code
	
	pushw	$s_apm_connecting - _start16
	call	print_string16
	add	$2, %sp
	
	movw    $0x5301, %ax    # Real Mode connect
	xorw    %bx, %bx
	int     $0x15
	jc      done_apm_bios   # error
	
	pushw	$s_apm_disabling - _start16
	call	print_string16
	add	$2, %sp
	
	movw    $0x5308, %ax    # Disable APM
	mov     $0xffff, %bx
	xorw    %cx, %cx
	int     $0x15

	pushw	$s_apm_disconnecting - _start16
	call	print_string16
	add	$2, %sp
	
	movw    $0x5304, %ax    # Do a final disconnect
	xorw    %bx, %bx
	int     $0x15           

done_apm_bios:	
	pushw	$s_apm_test_done - _start16
	call	print_string16
	add	$2, %sp
	
	popw	%bx
	popw	%bp
	retw

	
# Delay is needed after doing I/O
delay:
	.code16
	outb	%al,$0x80
	retw

halt16:
	.code16
	hlt
	jmp	halt16
	

print_string16:
	.code16
	pushw	%bp
	movw	%sp, %bp
	pushw	%si
	movw	4(%bp), %si
	xorw	%ax, %ax
print_string16.1:	
	lodsb %ds:(%si), %al
	testb	$0xff, %al
	jz	print_string16.2
	call	print_char16
	jmp	print_string16.1
print_string16.2:
	popw	%si
	popw	%bp
	ret

print_hex16:
	.code16
	pushw	%bp
	movw	%sp, %bp
	movw	$16, %cx
print_hex16.1:	
	movw	4(%bp), %ax
	subb	$4, %cl
	shrw	%cl, %ax
	andb	$0x0f, %al
	cmpb	$9, %al
	ja	print_hex16.2
	addb	$'0', %al
	jmp	print_hex16.3
print_hex16.2:
	addb	$'A' - 10, %al
print_hex16.3:
	pushw	%cx
	call	print_char16
	popw	%cx
	testb	%cl, %cl
	jnz	print_hex16.1

	popw	%bp
	ret

print_char16:
	.code16
	# The character to print is in al 
	call serial_print_char16
	retw


#define TTYS0_BASE	0x3f8
#define TTYS0_RBR	(TTYS0_BASE + 0x00)
#define TTYS0_TBR	(TTYS0_BASE + 0x00)
#define TTYS0_LSR	(TTYS0_BASE + 0x05)

serial_print_char16:
	.code16
	pushw	%bp
	movw	%sp, %bp
	# The character to print is in al 
	pushw	%ax
	
	# Wait until the serial port is ready to receive characters 
serial_print_char16.1:
	movw	$TTYS0_LSR, %dx
	inb	%dx, %al
	testb	$0x20, %al
	jz	serial_print_char16.1

	# Output the character 
	movw	$TTYS0_TBR, %dx
	movb	-2(%bp), %al
	outb	%al, %dx

	# Wait until the serial port has transmitted the character 
serial_print_char16.2:
	movw	$TTYS0_LSR, %dx
	inb	%dx, %al
	testb	$0x40, %al
	jz	serial_print_char16.2

	# Restore %eax 
	popw	%ax
	# Return to caller
	popw	%bp
	retw


s_a20_err_msg:
	.asciz	"A20 gate not responding!\r\n"

s_in_real_mode:
	.asciz	"In real mode.\r\n"
s_base_memory_size:
	.asciz	"Base memory size: "
s_interrupts_enabled:
	.asciz	"Interrupts enabled.\r\n"
s_a20_disabled:
	.asciz	"A20 disabled.\r\n"
s_a20_cant_disable:
	.asciz	"Can not A20 line.\r\n"
s_a20_enabled:
	.asciz	"A20 enabled\r\n"
s_interrupts_disabled:
	.asciz	"Interrupts disabled.\r\n"

s_meme820:	.asciz	"E820 Memory Map.\r\n"
s_at:		.asciz	" @ "
s_type:		.asciz	" type: "
s_space:	.asciz	" "
s_colon:	.asciz	":"
s_none:		.asciz	" none "
s_ok:		.asciz	" ok "
s_done:		.asciz	" done\r\n"

s_meme801:
	.asciz	"E801  Memory size: "
s_mem88:
	.asciz	"Mem88 Memory size: "

s_dasd_type:
	.asciz	"DASD type: "
s_equipment_list:
	.asciz	"Equiptment list: "
s_sysdesc:
	.asciz	"Sysdesc: "
s_edd:
	.asciz	"EDD: "
s_auto_repeat_rate:
	.asciz	"Setting auto repeat rate "


s_video_type:
	.asciz	"Video type: "
s_video_pre_ega:
	.asciz	"CGA/MDA/HGA"
s_video_ega:
	.asciz	"EGA"
s_video_vga:
	.asciz	"VGA"

s_cursor:
	.asciz	"Cursor Position(Row,Column): "

s_video_mode:
	.asciz	"Video Mode: "

s_testing_for_apm:	
	.asciz	"Testing for APM.\r\n"
s_apm_found_disconnecting:
	.asciz	"APM Found disconnecting.\r\n"
s_apm_connecting:
	.asciz	"APM connecting.\r\n"
s_apm_disabling:
	.asciz	"APM disabling.\r\n"
s_apm_disconnecting:
	.asciz	"APM disconnecting.\r\n"
s_apm_test_done:
	.asciz	"APM test done.\r\n"

s_crlf:	.asciz	"\r\n"



a20_tries:		.byte A20_ENABLE_LOOPS
a20_disable_tries:	.byte A20_DISABLE_LOOPS


e820nr:		.byte 0
e820_map:	.fill E820_MAX * E820_SIZE, 1, 0
