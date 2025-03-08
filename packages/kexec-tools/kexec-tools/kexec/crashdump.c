/*
 * crashdump.c: Architecture independent code for crashdump support.
 *
 * Created by: Vivek Goyal (vgoyal@in.ibm.com)
 * Copyright (C) IBM Corporation, 2005. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation (version 2 of the License).
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <elf.h>
#include "kexec.h"
#include "crashdump.h"
#include "kexec-syscall.h"

/* include "crashdump-elf.c" twice to create two functions from one */

#define ELF_WIDTH 64
#define FUNC crash_create_elf64_headers
#define EHDR Elf64_Ehdr
#define PHDR Elf64_Phdr
#include "crashdump-elf.c"
#undef ELF_WIDTH
#undef PHDR
#undef EHDR
#undef FUNC

#define ELF_WIDTH 32
#define FUNC crash_create_elf32_headers
#define EHDR Elf32_Ehdr
#define PHDR Elf32_Phdr
#include "crashdump-elf.c"
#undef ELF_WIDTH
#undef PHDR
#undef EHDR
#undef FUNC

unsigned long crash_architecture(struct crash_elf_info *elf_info)
{
	if (xen_present())
		return xen_architecture(elf_info);
	else
		return elf_info->machine;
}

/* Returns the physical address of start of crash notes buffer for a cpu. */
int get_crash_notes_per_cpu(int cpu, uint64_t *addr, uint64_t *len)
{
	char crash_notes[PATH_MAX];
	char crash_notes_size[PATH_MAX];
	char line[MAX_LINE];
	FILE *fp;
	struct stat cpu_stat;
	int count;
	unsigned long long temp;
	int fopen_errno;
	int stat_errno;

	*addr = 0;
	*len = 0;

	sprintf(crash_notes, "/sys/devices/system/cpu/cpu%d/crash_notes", cpu);
	fp = fopen(crash_notes, "r");
	if (!fp) {
		fopen_errno = errno;
		if (fopen_errno != ENOENT)
			die("Could not open \"%s\": %s\n", crash_notes,
			    strerror(fopen_errno));
		if (stat("/sys/devices", &cpu_stat)) {
			stat_errno = errno;
			if (stat_errno == ENOENT)
				die("\"/sys/devices\" does not exist. "
				    "Sysfs does not seem to be mounted. "
				    "Try mounting sysfs.\n");
			die("Could not open \"/sys/devices\": %s\n",
			    strerror(stat_errno));
		}
		/* CPU is not physically present.*/
		return -1;
	}
	if (!fgets(line, sizeof(line), fp))
		die("Cannot parse %s: %s\n", crash_notes, strerror(errno));
	count = sscanf(line, "%llx", &temp);
	if (count != 1)
		die("Cannot parse %s: %s\n", crash_notes, strerror(errno));
	*addr = (uint64_t) temp;
	fclose(fp);

	*len = MAX_NOTE_BYTES;
	sprintf(crash_notes_size,
		"/sys/devices/system/cpu/cpu%d/crash_notes_size", cpu);
	fp = fopen(crash_notes_size, "r");
	if (fp) {
		if (!fgets(line, sizeof(line), fp))
			die("Cannot parse %s: %s\n",
			    crash_notes_size, strerror(errno));
		count = sscanf(line, "%llu", &temp);
		if (count != 1)
			die("Cannot parse %s: %s\n",
			    crash_notes_size, strerror(errno));
		*len = (uint64_t) temp;
		fclose(fp);
	}

	dbgprintf("%s: crash_notes addr = %llx, size = %llu\n", __FUNCTION__,
		  (unsigned long long)*addr, (unsigned long long)*len);

	return 0;
}

static int get_vmcoreinfo(const char *kdump_info, uint64_t *addr, uint64_t *len)
{
	char line[MAX_LINE];
	int count;
	FILE *fp;
	unsigned long long temp, temp2;

	*addr = 0;
	*len = 0;

	if (!(fp = fopen(kdump_info, "r")))
		return -1;

	if (!fgets(line, sizeof(line), fp))
		die("Cannot parse %s: %s\n", kdump_info, strerror(errno));
	count = sscanf(line, "%llx %llx", &temp, &temp2);
	if (count != 2)
		die("Cannot parse %s: %s\n", kdump_info, strerror(errno));

	*addr = (uint64_t) temp;
	*len = (uint64_t) temp2;

	fclose(fp);
	return 0;
}

/* Returns the physical address of start of crash notes buffer for a kernel. */
int get_kernel_vmcoreinfo(uint64_t *addr, uint64_t *len)
{
	return get_vmcoreinfo("/sys/kernel/vmcoreinfo", addr, len);
}
