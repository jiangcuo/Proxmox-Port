#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "kexec.h"
#include "crashdump.h"

/*
 * kexec_iomem_for_each_line()
 *
 * Iterate over each line in the file returned by proc_iomem(). If match is
 * NULL or if the line matches with our match-pattern then call the
 * callback if non-NULL.
 * If match is NULL, callback should return a negative if error.
 * Otherwise the interation goes on, incrementing nr but only if callback
 * returns 0 (matched).
 *
 * Return the number of lines matched.
 */

int kexec_iomem_for_each_line(char *match,
			      int (*callback)(void *data,
					      int nr,
					      char *str,
					      unsigned long long base,
					      unsigned long long length),
			      void *data)
{
	const char *iomem = proc_iomem();
	char line[MAX_LINE];
	FILE *fp;
	unsigned long long start, end, size;
	char *str;
	int consumed;
	int count;
	int nr = 0, ret;

	if (!callback)
		return nr;

	fp = fopen(iomem, "r");
	if (!fp)
		die("Cannot open %s\n", iomem);

	while(fgets(line, sizeof(line), fp) != 0) {
		count = sscanf(line, "%llx-%llx : %n", &start, &end, &consumed);
		if (count != 2)
			continue;
		str = line + consumed;
		size = end - start + 1;
		if (!match || memcmp(str, match, strlen(match)) == 0) {
			ret = callback(data, nr, str, start, size);
			if (ret < 0)
				break;
			else if (ret == 0)
				nr++;
		}
	}

	fclose(fp);

	return nr;
}

static int kexec_iomem_single_callback(void *data, int nr,
				       char *UNUSED(str),
				       unsigned long long base,
				       unsigned long long length)
{
	struct memory_range *range = data;

	if (nr == 0) {
		range->start = base;
		range->end = base + length - 1;
	}

	return 0;
}

int parse_iomem_single(char *str, uint64_t *start, uint64_t *end)
{
	struct memory_range range;
	int ret;

	memset(&range, 0, sizeof(range));

	ret = kexec_iomem_for_each_line(str,
	                                kexec_iomem_single_callback, &range);

	if (ret == 1) {
		if (start)
			*start = range.start;
		if (end)
			*end = range.end;

		ret = 0;
	}
	else
		ret = -1;

	return ret;
}
