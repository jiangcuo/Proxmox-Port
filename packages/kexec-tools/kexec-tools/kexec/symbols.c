#include <stdio.h>
#include <string.h>
#include "kexec.h"

/* Retrieve kernel symbol virtual address from /proc/kallsyms */
unsigned long long get_kernel_sym(const char *symbol)
{
	const char *kallsyms = "/proc/kallsyms";
	char sym[128];
	char line[128];
	FILE *fp;
	unsigned long long vaddr;
	char type;

	fp = fopen(kallsyms, "r");
	if (!fp) {
		fprintf(stderr, "Cannot open %s\n", kallsyms);
		return 0;
	}

	while (fgets(line, sizeof(line), fp) != NULL) {
		if (sscanf(line, "%llx %c %s", &vaddr, &type, sym) != 3)
			continue;
		if (strcmp(sym, symbol) == 0) {
			dbgprintf("kernel symbol %s vaddr = %16llx\n",
					symbol, vaddr);
			fclose(fp);
			return vaddr;
		}
	}

	dbgprintf("Cannot get kernel %s symbol address\n", symbol);

	fclose(fp);
	return 0;
}
