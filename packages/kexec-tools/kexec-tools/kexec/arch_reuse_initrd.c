#include "kexec.h"

unsigned char reuse_initrd = 0;

void arch_reuse_initrd(void)
{
	die("--reuseinitrd not implemented on this architecture\n");
}
