#include "kexec.h"

static const char proc_iomem_str[] = "/proc/iomem";

/*
 * Allow an architecture specific implementation of this
 * function to override the location of a file looking a lot
 * like /proc/iomem
 */
const char *proc_iomem(void)
{
        return proc_iomem_str;
}
