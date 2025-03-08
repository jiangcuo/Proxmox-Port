/*
 * kexec/arch/s390/crashdump-s390.c
 *
 * Copyright IBM Corp. 2011
 *
 * Author(s): Michael Holzheu <holzheu@linux.vnet.ibm.com>
 */

#ifdef __s390x__
#define _GNU_SOURCE

#include <stdio.h>
#include <elf.h>
#include <limits.h>
#include <string.h>
#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "../../kexec/crashdump.h"
#include "kexec-s390.h"

/*
 * Create ELF core header
 */
static int create_elf_header(struct kexec_info *info, unsigned long crash_base,
			     unsigned long crash_end)
{
#ifdef WITH_ELF_HEADER
	static struct memory_range crash_memory_range[MAX_MEMORY_RANGES];
	unsigned long elfcorehdr, elfcorehdr_size, bufsz;
	struct crash_elf_info elf_info;
	char str[COMMAND_LINESIZE];
	int ranges;
	void *tmp;

	memset(&elf_info, 0, sizeof(elf_info));

	elf_info.data = ELFDATA2MSB;
	elf_info.machine = EM_S390;
	elf_info.class = ELFCLASS64;
	elf_info.get_note_info = get_crash_notes_per_cpu;

	if (get_memory_ranges_s390(crash_memory_range, &ranges, 0))
		return -1;

	if (crash_create_elf64_headers(info, &elf_info, crash_memory_range,
				       ranges, &tmp, &bufsz,
				       ELF_CORE_HEADER_ALIGN))
		return -1;

	elfcorehdr = add_buffer(info, tmp, bufsz, bufsz, 1024,
				crash_base, crash_end, -1);
	elfcorehdr_size = bufsz;
	snprintf(str, sizeof(str), " elfcorehdr=%ld@%ldK\n",
		 elfcorehdr_size, elfcorehdr / 1024);
	if (command_line_add(info, str))
		return -1;
#endif
	return 0;
}

/*
 * Load additional segments for kdump kernel
 */
int load_crashdump_segments(struct kexec_info *info, unsigned long crash_base,
			    unsigned long crash_end)
{
	unsigned long crash_size = crash_size = crash_end - crash_base + 1;

	if (create_elf_header(info, crash_base, crash_end))
		return -1;
	elf_rel_build_load(info, &info->rhdr, (const char *) purgatory,
			   purgatory_size, crash_base + 0x2000,
			   crash_base + 0x10000, -1, 0);
	elf_rel_set_symbol(&info->rhdr, "crash_base", &crash_base,
			   sizeof(crash_base));
	elf_rel_set_symbol(&info->rhdr, "crash_size", &crash_size,
			   sizeof(crash_size));
	info->entry = (void *) elf_rel_get_addr(&info->rhdr, "purgatory_start");
	return 0;
}
#else
/*
 * kdump is not available for s390
 */
int load_crashdump_segments(struct kexec_info *info, unsigned long crash_base,
			    unsigned long crash_end)
{
	return -1;
}
#endif
