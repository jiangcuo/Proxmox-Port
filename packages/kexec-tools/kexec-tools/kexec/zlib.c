#include "kexec-zlib.h"
#include "kexec.h"

#ifdef HAVE_LIBZ
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
#include <ctype.h>
#include <zlib.h>

static void _gzerror(gzFile fp, int *errnum, const char **errmsg)
{
	*errmsg = gzerror(fp, errnum);
	if (*errnum == Z_ERRNO) {
		*errmsg = strerror(*errnum);
	}
}

int is_zlib_file(const char *filename, off_t *r_size)
{
	gzFile fp;
	int errnum;
	int is_zlib_file = 0; /* default: It's not in gzip format */
	const char *msg;
	ssize_t result;

	if (!filename)
		goto out;

	fp = gzopen(filename, "rb");
	if (fp == 0) {
		_gzerror(fp, &errnum, &msg);
		dbgprintf("Cannot open `%s': %s\n", filename, msg);
		goto out;
	}

	if (!gzdirect(fp))
		/* It's in gzip format */
		is_zlib_file = 1;

	result = gzclose(fp);
	if (result != Z_OK) {
		_gzerror(fp, &errnum, &msg);
		dbgprintf(" Close of %s failed: %s\n", filename, msg);
	}

out:
	return is_zlib_file;
}

char *zlib_decompress_file(const char *filename, off_t *r_size)
{
	gzFile fp;
	int errnum;
	const char *msg;
	char *buf = NULL;
	off_t size = 0, allocated;
	ssize_t result;

	dbgprintf("Try gzip decompression.\n");

	*r_size = 0;
	if (!filename) {
		return NULL;
	}
	fp = gzopen(filename, "rb");
	if (fp == 0) {
		_gzerror(fp, &errnum, &msg);
		dbgprintf("Cannot open `%s': %s\n", filename, msg);
		return NULL;
	}
	if (gzdirect(fp)) {
		/* It's not in gzip format */
		goto fail;
	}
	allocated = 65536;
	buf = xmalloc(allocated);
	do {
		if (size == allocated) {
			allocated <<= 1;
			buf = xrealloc(buf, allocated);
		}
		result = gzread(fp, buf + size, allocated - size);
		if (result < 0) {
			if ((errno == EINTR) || (errno == EAGAIN))
				continue;
			_gzerror(fp, &errnum, &msg);
			dbgprintf("Read on %s of %d bytes failed: %s\n",
				  filename, (int)(allocated - size), msg);
			size = 0;
			goto fail;
		}
		size += result;
	} while(result > 0);

fail:
	result = gzclose(fp);
	if (result != Z_OK) {
		_gzerror(fp, &errnum, &msg);
		dbgprintf(" Close of %s failed: %s\n", filename, msg);
	}

	if (size > 0) {
		*r_size = size;
	} else {
		free(buf);
		buf = NULL;
	}
	return buf;
}
#else

int is_zlib_file(const char *filename, off_t *r_size)
{
	return 0;
}

char *zlib_decompress_file(const char *UNUSED(filename), off_t *UNUSED(r_size))
{
	return NULL;
}
#endif /* HAVE_ZLIB */
