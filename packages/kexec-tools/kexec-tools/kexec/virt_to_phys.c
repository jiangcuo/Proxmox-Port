#include "kexec.h"
#include <stdlib.h>

unsigned long virt_to_phys(unsigned long UNUSED(addr))
{
	abort();
}
