/*
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

/*  pugatory relocation code
 *  Most of the code in this file is
 *  based on arch/ia64/kernel/module.c in Linux kernel
 */


/*  Most of the code in this file is
 *  based on arch/ia64/kernel/module.c in Linux kernel
 */

#include <stdio.h>
#include <elf.h>
#include "../../kexec.h"
#include "../../kexec-elf.h"

#define MAX_LTOFF       ((uint64_t) (1 << 22))

int machine_verify_elf_rel(struct mem_ehdr *ehdr)
{
	if (ehdr->ei_data != ELFDATA2LSB) {
		return 0;
	}
	if (ehdr->ei_class != ELFCLASS64) {
		return 0;
	}
	if (ehdr->e_machine != EM_IA_64) {
		return 0;
	}
	return 1;
}

static void
ia64_patch (uint64_t insn_addr, uint64_t mask, uint64_t val)
{
        uint64_t m0, m1, v0, v1, b0, b1, *b = (uint64_t *) (insn_addr & -16);
#       define insn_mask ((1UL << 41) - 1)
        unsigned long shift;

        b0 = b[0]; b1 = b[1];
        shift = 5 + 41 * (insn_addr % 16); /* 5 bits of template, then 3 x 41-bit instructions */
        if (shift >= 64) {
                m1 = mask << (shift - 64);
                v1 = val << (shift - 64);
        } else {
                m0 = mask << shift; m1 = mask >> (64 - shift);
                v0 = val  << shift; v1 = val >> (64 - shift);
                b[0] = (b0 & ~m0) | (v0 & m0);
        }
        b[1] = (b1 & ~m1) | (v1 & m1);
}

static inline uint64_t
bundle (const uint64_t insn)
{
        return insn & ~0xfUL;
}

void machine_apply_elf_rel(struct mem_ehdr *ehdr,
	struct mem_sym *UNUSED(sym), unsigned long r_type, void *location,
	unsigned long address, unsigned long value)
{
	uint64_t gp_value = ehdr->rel_addr + 0x200000;
	switch(r_type) {
	case R_IA64_NONE:
		break;
	case R_IA64_SEGREL64LSB:
	case R_IA64_DIR64LSB:
		*((uint64_t *)location) = value;
		break;
	case R_IA64_DIR32LSB:
		*((uint32_t *)location) = value;
		if (value != *((uint32_t *)location))
			goto overflow;
		break;
	case R_IA64_IMM64:
		ia64_patch((uint64_t)location, 0x01fffefe000UL,
				/* bit 63 -> 36 */
				(((value & 0x8000000000000000UL) >> 27)
				/* bit 21 -> 21 */
				  | ((value & 0x0000000000200000UL) <<  0)
				/* bit 16 -> 22 */
				  | ((value & 0x00000000001f0000UL) <<  6)
				/* bit 7 -> 27 */
				  | ((value & 0x000000000000ff80UL) << 20)
				/* bit 0 -> 13 */
				  | ((value & 0x000000000000007fUL) << 13)));
		ia64_patch((uint64_t)location - 1, 0x1ffffffffffUL, value>>22);
		break;
	case R_IA64_IMM22:
		if (value + (1 << 21) >= (1 << 22))
                	die("value out of IMM22 range\n");
		ia64_patch((uint64_t)location, 0x01fffcfe000UL,
				/* bit 21 -> 36 */
				(((value & 0x200000UL) << 15)
				/* bit 16 -> 22 */
				 | ((value & 0x1f0000UL) <<  6)
				/* bit  7 -> 27 */
				 | ((value & 0x00ff80UL) << 20)
				/* bit  0 -> 13 */
				 | ((value & 0x00007fUL) << 13) ));
		break;
	case R_IA64_PCREL21B: {
		uint64_t delta = ((int64_t)value - (int64_t)address)/16;
		if (delta + (1 << 20) >= (1 << 21))
			die("value out of IMM21B range\n");
		value = ((int64_t)(value - bundle(address)))/16;
		ia64_patch((uint64_t)location, 0x11ffffe000UL,
				(((value & 0x100000UL) << 16) /* bit 20 -> 36 */
				 | ((value & 0x0fffffUL) << 13) /* bit  0 -> 13 */));
		}
		break;
	case R_IA64_PCREL64LSB: {
		value = value - address;
		put_unaligned(value, (uint64_t *)location);
	} break;
	case R_IA64_GPREL22:
	case R_IA64_LTOFF22X:
		if (value - gp_value + MAX_LTOFF/2 >= MAX_LTOFF)
			die("value out of gp relative range");
		value -= gp_value;
		ia64_patch((uint64_t)location, 0x01fffcfe000UL,
				(((value & 0x200000UL) << 15) /* bit 21 -> 36 */
				   |((value & 0x1f0000UL) <<  6) /* bit 16 -> 22 */
				   |((value & 0x00ff80UL) << 20) /* bit  7 -> 27 */
				   |((value & 0x00007fUL) << 13) /* bit  0 -> 13 */));
		break;
	case R_IA64_LDXMOV:
		if (value - gp_value + MAX_LTOFF/2 >= MAX_LTOFF)
			die("value out of gp relative range");
		ia64_patch((uint64_t)location, 0x1fff80fe000UL, 0x10000000000UL);
	        break;
	case R_IA64_LTOFF22:

	default:
		die("Unknown rela relocation: 0x%lx 0x%lx\n",
				r_type, address);
		break;
	}
	return;
overflow:
	die("overflow in relocation type %lu val %llx\n",
			r_type, value);
}
