#include "kexec.h"
#include <errno.h>
#include <string.h>
#include <sys/utsname.h>

long physical_arch(void)
{
	struct utsname utsname;
	int i, result = uname(&utsname);
	if (result < 0) {
		fprintf(stderr, "uname failed: %s\n",
			strerror(errno));
		return -1;
	}

	for (i = 0; arches[i].machine; ++i) {
		if (strcmp(utsname.machine, arches[i].machine) == 0)
			return arches[i].arch;
		if ((strcmp(arches[i].machine, "arm") == 0) &&
		    (strncmp(utsname.machine, arches[i].machine,
		     strlen(arches[i].machine)) == 0))
			return arches[i].arch;
	}

	fprintf(stderr, "Unsupported machine type: %s\n",
		utsname.machine);
	return -1;
}
