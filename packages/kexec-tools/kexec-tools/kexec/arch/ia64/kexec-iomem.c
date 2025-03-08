#include <stdint.h>
#include <stdio.h>
#include "../../kexec.h"
#include "../../crashdump.h"

static const char proc_iomem_str[]= "/proc/iomem";
static const char proc_iomem_machine_str[]= "/proc/iomem_machine";

/*
 * On IA64 XEN the EFI tables are virtualised.
 * For this reason on such systems /proc/iomem_machine is provided,
 * which is based on the hypervisor's (machine's) EFI tables.
 * If Xen is in use, then /proc/iomem is used for memory regions relating
 * to the currently running dom0 kernel, and /proc/iomem_machine is used
 * for regions relating to the machine itself or the hypervisor.
 * If Xen is not in used, then /proc/iomem used.
 */
const char *proc_iomem(void)
{
	if (xen_present())
		return proc_iomem_machine_str;
	return proc_iomem_str;
}
