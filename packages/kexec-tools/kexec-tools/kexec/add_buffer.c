#include "kexec.h"

unsigned long add_buffer(struct kexec_info *info,
			 const void *buf,
			 unsigned long bufsz,
			 unsigned long memsz,
			 unsigned long buf_align,
			 unsigned long buf_min,
			 unsigned long buf_max,
			 int buf_end)
{
	return add_buffer_virt(info, buf, bufsz, memsz, buf_align,
			       buf_min, buf_max, buf_end);
}
