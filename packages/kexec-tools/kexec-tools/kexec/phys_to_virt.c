#include "kexec.h"
#include "crashdump.h"

/**
 * phys_to_virt() - translate physical address to virtual address
 * @paddr: physical address to translate
 *
 * For most architectures physical address is simply virtual address minus
 * PAGE_OFFSET. Architectures that don't follow this convention should provide
 * their own implementation.
 */
unsigned long
phys_to_virt(struct crash_elf_info *elf_info, unsigned long long paddr)
{
	return paddr + elf_info->page_offset;
}
