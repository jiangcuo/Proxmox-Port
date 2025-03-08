#include "../../kexec.h"
#include "../../crashdump.h"
#include "phys_to_virt.h"

uint64_t phys_offset;

/**
 * phys_to_virt() - translate physical address to virtual address
 * @paddr: physical address to translate
 *
 * For ARM we have following equation to translate from virtual address to
 * physical:
 *	paddr = vaddr - PAGE_OFFSET + PHYS_OFFSET
 *
 * See also:
 * http://lists.arm.linux.org.uk/lurker/message/20010723.185051.94ce743c.en.html
 */
unsigned long
phys_to_virt(struct crash_elf_info *elf_info, unsigned long long paddr)
{
	return paddr + elf_info->page_offset - phys_offset;
}
