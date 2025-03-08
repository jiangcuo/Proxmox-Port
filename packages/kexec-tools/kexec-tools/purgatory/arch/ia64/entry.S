/*
 * purgatory:  setup code
 *
 * Copyright (C) 2005-2006  Zou Nan hai (nanhai.zou@intel.com)
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
#define DECLARE_DATA8(name)	\
.global name;	\
.size	name, 8;	\
name:	data8 0x0

.global __dummy_efi_function
.align  32
.proc  __dummy_efi_function
__dummy_efi_function:
	mov r8=r0;;
	br.ret.sptk.many rp;;
.global __dummy_efi_function_end
__dummy_efi_function_end:
.endp 	__dummy_efi_function

.global purgatory_start
.align  32
.proc   purgatory_start
purgatory_start:
	movl r2=__gp_value;;
	ld8 gp=[r2];;
	br.call.sptk.many b0=purgatory
	;;
	alloc r2 = ar.pfs, 0, 0, 2, 0
	;;
	mov out0=r28
	movl out1=__vmcode_base;
	br.call.sptk.many b0=ia64_env_setup
	movl r10=__kernel_entry;;
	ld8 r14=[r10];;
	movl r10=__boot_param_base;;
	ld8 r28=[r10];;
	mov b6=r14;;
	mov ar.lc=r0
	mov ar.ec=r0
	cover;;
	invala;;
	br.call.sptk.many  b0=b6
.endp   purgatory_start

DECLARE_DATA8(__kernel_entry)
DECLARE_DATA8(__vmcode_base)
DECLARE_DATA8(__vmcode_size)
DECLARE_DATA8(__ramdisk_base)
DECLARE_DATA8(__ramdisk_size)
DECLARE_DATA8(__command_line)
DECLARE_DATA8(__command_line_len)
DECLARE_DATA8(__efi_memmap_base)
DECLARE_DATA8(__efi_memmap_size)
DECLARE_DATA8(__boot_param_base)
DECLARE_DATA8(__loaded_segments)
DECLARE_DATA8(__loaded_segments_num)

DECLARE_DATA8(__gp_value)
DECLARE_DATA8(__noio)
