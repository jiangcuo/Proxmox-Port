/*
 * purgatory:  stack
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

	/* A stack for the loaded kernel.
	 * Seperate and in the data section so it can be prepopulated.
	 */
	.data
	.balign 4096
	.globl stack, stack_end
	.globl stack_arg32_1, stack_arg32_2, stack_arg32_3 ,stack_arg32_4
	.globl stack_arg32_5, stack_arg32_6, stack_arg32_7 ,stack_arg32_8
	.globl stack_arg64_1, stack_arg64_2, stack_arg64_3 ,stack_arg64_4

stack:
	.skip 4096 - (8*4)
stack_arg64_4:		; .size stack_arg64_4, 8
stack_arg32_8:	.long 0 ; .size stack_arg32_8, 4
stack_arg32_7:	.long 0 ; .size stack_arg32_7, 4
stack_arg64_3:	       	; .size stack_arg64_3, 8
stack_arg32_6:	.long 0	; .size stack_arg32_6, 4	
stack_arg32_5:	.long 0	; .size stack_arg32_5, 4	
stack_arg64_2:	       	; .size stack_arg64_2, 8
stack_arg32_4:	.long 0	; .size stack_arg32_4, 4	
stack_arg32_3:	.long 0 ; .size stack_arg32_3, 4
stack_arg64_1:		; .size stack_arg64_1, 8
stack_arg32_2:	.long 0	; .size stack_arg32_2, 4
stack_arg32_1:	.long 0	; .size stack_arg32_1, 4
stack_end:		
