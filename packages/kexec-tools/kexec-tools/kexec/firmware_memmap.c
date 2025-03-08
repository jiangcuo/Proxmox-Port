/*
 * firmware_memmap.c: Read /sys/firmware/memmap
 *
 * Created by: Bernhard Walle (bernhard.walle@gmx.de)
 * Copyright (C) SUSE LINUX Products GmbH, 2008. All rights reserved
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
#define _GNU_SOURCE /* for ULLONG_MAX without C99 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "firmware_memmap.h"
#include "kexec.h"

/*
 * If the system is too old for ULLONG_MAX or LLONG_MAX, define it here.
 */
#ifndef ULLONG_MAX
#    define ULLONG_MAX (~0ULL)
#endif /* ULLONG_MAX */

#ifndef LLONG_MAX
#    define LLONG_MAX (~0ULL >> 1)
#endif /* LLONG_MAX */


/**
 * The full path to the sysfs interface that provides the memory map.
 */
#define FIRMWARE_MEMMAP_DIR  "/sys/firmware/memmap"

/**
 * Parses a file that only contains one number. Typical for sysfs files.
 *
 * @param[in] filename the name of the file that should be parsed
 * @return the value that has been read or ULLONG_MAX on error.
 */
static unsigned long long parse_numeric_sysfs(const char *filename)
{
	FILE *fp;
	char linebuffer[BUFSIZ];
	unsigned long long retval = ULLONG_MAX;

	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "Opening \"%s\" failed: %s\n",
			filename, strerror(errno));
		return ULLONG_MAX;
	}

	if (!fgets(linebuffer, BUFSIZ, fp))
		goto err;

	linebuffer[BUFSIZ-1] = 0;

	/* let strtoll() detect the base */
	retval = strtoll(linebuffer, NULL, 0);

err:
	fclose(fp);

	return retval;
}

/**
 * Reads the contents of a one-line sysfs file to buffer. (This function is
 * not threadsafe.)
 *
 * @param[in] filename the name of the file that should be read
 *
 * @return NULL on failure, a pointer to a static buffer (that should be copied
 *         with strdup() if the caller plans to use it after next function call)
 */
static char *parse_string_sysfs(const char *filename)
{
	FILE *fp;
	static char linebuffer[BUFSIZ];
	char *end;

	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "Opening \"%s\" failed: %s\n",
			filename, strerror(errno));
		return NULL;
	}

	if (!fgets(linebuffer, BUFSIZ, fp)) {
		fclose(fp);
		return NULL;
	}

	linebuffer[BUFSIZ-1] = 0;

	/* truncate trailing newline(s) */
	end = linebuffer + strlen(linebuffer) - 1;
	while (*end == '\n')
		*end-- = 0;

	fclose(fp);

	return linebuffer;

}

static int parse_memmap_entry(const char *entry, struct memory_range *range)
{
	char filename[PATH_MAX];
	char *type;
	int ret;

	/*
	 * entry/start
	 */
	ret = snprintf(filename, PATH_MAX, "%s/%s", entry, "start");
	if (ret < 0 || ret >= PATH_MAX) {
		fprintf(stderr, "snprintf failed: %s\n", strerror(errno));
		return -1;
	}

	filename[PATH_MAX-1] = 0;

	range->start = parse_numeric_sysfs(filename);
	if (range->start == ULLONG_MAX)
		return -1;

	/*
	 * entry/end
	 */
	ret = snprintf(filename, PATH_MAX, "%s/%s", entry, "end");
	if (ret < 0 || ret >= PATH_MAX) {
		fprintf(stderr, "snprintf failed: %s\n", strerror(errno));
		return -1;
	}

	filename[PATH_MAX-1] = 0;

	range->end = parse_numeric_sysfs(filename);
	if (range->end == ULLONG_MAX)
		return -1;

	/*
	 * entry/type
	 */
	ret = snprintf(filename, PATH_MAX, "%s/%s", entry, "type");
	if (ret < 0 || ret >= PATH_MAX) {
		fprintf(stderr, "snprintf failed: %s\n", strerror(errno));
		return -1;
	}

	filename[PATH_MAX-1] = 0;

	type = parse_string_sysfs(filename);
	if (!type)
		return -1;

	if (strcmp(type, "System RAM") == 0)
		range->type = RANGE_RAM;
	else if (strcmp(type, "ACPI Tables") == 0)
		range->type = RANGE_ACPI;
	else if (strcmp(type, "Unusable memory") == 0)
		range->type = RANGE_RESERVED;
	else if (strcmp(type, "reserved") == 0)
		range->type = RANGE_RESERVED;
	else if (strcmp(type, "Reserved") == 0)
		range->type = RANGE_RESERVED;
	else if (strcmp(type, "Unknown E820 type") == 0)
		range->type = RANGE_RESERVED;
	else if (strcmp(type, "ACPI Non-volatile Storage") == 0)
		range->type = RANGE_ACPI_NVS;
	else if (strcmp(type, "Uncached RAM") == 0)
		range->type = RANGE_UNCACHED;
	else if (strcmp(type, "Persistent Memory (legacy)") == 0)
		range->type = RANGE_PRAM;
	else if (strcmp(type, "Persistent Memory") == 0)
		range->type = RANGE_PMEM;
	else {
		fprintf(stderr, "Unknown type (%s) while parsing %s. Please "
			"report this as bug. Using RANGE_RESERVED now.\n",
			type, filename);
		range->type = RANGE_RESERVED;
	}

	return 0;
}

/* documentation: firmware_memmap.h */
int compare_ranges(const void *first, const void *second)
{
	const struct memory_range *first_range = first;
	const struct memory_range *second_range = second;

	/*
	 * don't use the "first_range->start - second_range->start"
	 * notation because unsigned long long might overflow
	 */
	if (first_range->start > second_range->start)
		return 1;
	else if (first_range->start < second_range->start)
		return -1;
	else /* first_range->start == second_range->start */
		return 0;
}

/* documentation: firmware_memmap.h */
int have_sys_firmware_memmap(void)
{
	int ret;
	struct stat mystat;

	ret = stat(FIRMWARE_MEMMAP_DIR, &mystat);
	if (ret != 0)
		return 0;

	return S_ISDIR(mystat.st_mode);
}

/* documentation: firmware_memmap.h */
int get_firmware_memmap_ranges(struct memory_range *range, size_t *ranges)
{
	DIR *firmware_memmap_dir = NULL;
	struct dirent *dirent;
	int i = 0;

	/* argument checking */
	if (!range || !ranges) {
		fprintf(stderr, "%s: Invalid arguments.\n", __FUNCTION__);
		return -1;
	}

	/* open the directory */
	firmware_memmap_dir = opendir(FIRMWARE_MEMMAP_DIR);
	if (!firmware_memmap_dir) {
		perror("Could not open \"" FIRMWARE_MEMMAP_DIR "\"");
		goto error;
	}

	/* parse the entries */
	while ((dirent = readdir(firmware_memmap_dir)) != NULL) {
		int ret;
		char full_path[PATH_MAX];

		/* array overflow check */
		if ((size_t)i >= *ranges) {
			fprintf(stderr, "The firmware provides more entries "
				"allowed (%zd). Please report that as bug.\n",
				*ranges);
			goto error;
		}

		/* exclude '.' and '..' */
		if (dirent->d_name[0] && dirent->d_name[0] == '.') {
			continue;
		}

		snprintf(full_path, PATH_MAX, "%s/%s", FIRMWARE_MEMMAP_DIR,
			dirent->d_name);
		full_path[PATH_MAX-1] = 0;
		ret = parse_memmap_entry(full_path, &range[i]);
		if (ret < 0) {
			goto error;
		}

		i++;
	}

	/* close the dir as we don't need it any more */
	closedir(firmware_memmap_dir);

	/* update the number of ranges for the caller */
	*ranges = i;

	/* and finally sort the entries with qsort */
	qsort(range, *ranges, sizeof(struct memory_range), compare_ranges);

	return 0;

error:
	if (firmware_memmap_dir) {
		closedir(firmware_memmap_dir);
	}
	return -1;
}

