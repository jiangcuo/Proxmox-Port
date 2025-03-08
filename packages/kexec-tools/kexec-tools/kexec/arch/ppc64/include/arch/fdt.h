#ifndef KEXEC_ARCH_PPC64_FDT
#define KEXEC_ARCH_PPC64_FDT

#include <sys/types.h>

int fixup_dt(char **fdt, off_t *size, unsigned long kexec_flags);

#endif
