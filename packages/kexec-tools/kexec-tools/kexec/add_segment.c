#include "kexec.h"

void add_segment(struct kexec_info *info,
		 const void *buf, size_t bufsz,
		 unsigned long base, size_t memsz)
{
	return add_segment_phys_virt(info, buf, bufsz, base, memsz, 0);
}
