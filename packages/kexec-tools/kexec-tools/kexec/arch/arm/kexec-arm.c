/*
 * kexec: Linux boots Linux
 *
 * modified from kexec-ppc.c
 *
 */

#define _GNU_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "kexec-arm.h"
#include <arch/options.h>
#include "../../fs2dt.h"
#include "iomem.h"

#define MAX_MEMORY_RANGES 64
#define MAX_LINE 160
static struct memory_range memory_range[MAX_MEMORY_RANGES];

/* Return a sorted list of available memory ranges. */
int get_memory_ranges(struct memory_range **range, int *ranges,
		unsigned long UNUSED(kexec_flags))
{
	const char *iomem = proc_iomem();
	int memory_ranges = 0;
	char line[MAX_LINE];
	FILE *fp;
	fp = fopen(iomem, "r");
	if (!fp) {
		fprintf(stderr, "Cannot open %s: %s\n",
			iomem, strerror(errno));
		return -1;
	}

	while(fgets(line, sizeof(line), fp) != 0) {
		unsigned long long start, end;
		char *str;
		int type;
		int consumed;
		int count;
		if (memory_ranges >= MAX_MEMORY_RANGES)
			break;
		count = sscanf(line, "%llx-%llx : %n",
			&start, &end, &consumed);
		if (count != 2)
			continue;
		str = line + consumed;

		if (memcmp(str, SYSTEM_RAM_BOOT, strlen(SYSTEM_RAM_BOOT)) == 0 ||
		    memcmp(str, SYSTEM_RAM, strlen(SYSTEM_RAM)) == 0) {
			type = RANGE_RAM;
		}
		else if (memcmp(str, "reserved\n", 9) == 0) {
			type = RANGE_RESERVED;
		}
		else {
			continue;
		}

		memory_range[memory_ranges].start = start;
		memory_range[memory_ranges].end = end;
		memory_range[memory_ranges].type = type;
		memory_ranges++;
	}
	fclose(fp);
	*range = memory_range;
	*ranges = memory_ranges;

	dbgprint_mem_range("MEMORY RANGES", *range, *ranges);

	return 0;
}

/* Supported file types and callbacks */
struct file_type file_type[] = {
	/* uImage is probed before zImage because the latter also accepts
	   uncompressed images. */
	{"uImage", uImage_arm_probe, uImage_arm_load, zImage_arm_usage},
	{"zImage", zImage_arm_probe, zImage_arm_load, zImage_arm_usage},
};
int file_types = sizeof(file_type) / sizeof(file_type[0]);

void arch_usage(void)
{
	printf("     --image-size=<size>\n"
	       "               Specify the assumed total image size of\n"
	       "               the kernel that is about to be loaded,\n"
	       "               including the .bss section, as reported\n"
	       "               by 'arm-linux-size vmlinux'. If not\n"
	       "               specified, this value is implicitly set\n"
	       "               to the compressed images size * 4.\n"
	       "     --dt-no-old-root\n"
	       "               do not reuse old kernel root= param.\n"
	       "               while creating flatten device tree.\n");
}

int arch_process_options(int argc, char **argv)
{
	/* We look for all options so getopt_long doesn't start reordering
	 * argv[] before file_type[n].load() gets a look in.
	 */
	static const struct option options[] = {
		KEXEC_ALL_OPTIONS
		{ 0, 0, NULL, 0 },
	};
	static const char short_options[] = KEXEC_ALL_OPT_STR;
	int opt;

	opterr = 0; /* Don't complain about unrecognized options here */
	while((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch(opt) {
		case OPT_DT_NO_OLD_ROOT:
			dt_no_old_root = 1;
			break;
		default:
			break;
		}
	}
	/* Reset getopt for the next pass; called in other source modules */
	opterr = 1;
	optind = 1;
	return 0;
}

const struct arch_map_entry arches[] = {
	{ "arm", KEXEC_ARCH_ARM },
	{ NULL, 0 },
};

int arch_compat_trampoline(struct kexec_info *UNUSED(info))
{
	return 0;
}

void arch_update_purgatory(struct kexec_info *UNUSED(info))
{
}

/* return 1 if /sys/firmware/fdt exists, otherwise return 0 */
int have_sysfs_fdt(void)
{
	return !access(SYSFS_FDT, F_OK);
}

int arch_do_exclude_segment(struct kexec_info *UNUSED(info), struct kexec_segment *UNUSED(segment))
{
	return 0;
}
