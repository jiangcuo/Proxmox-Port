#include <unistd.h>
#include <sys/types.h>

#include "kexec-lzma.h"
#include "config.h"
#include "kexec.h"

#ifdef HAVE_LIBLZMA
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <ctype.h>
#include <lzma.h>

#define kBufferSize (1 << 15)

typedef struct lzfile {
	uint8_t buf[kBufferSize];
	lzma_stream strm;
	FILE *file;
	int encoding;
	int eof;
} LZFILE;

LZFILE *lzopen(const char *path, const char *mode);
int lzclose(LZFILE *lzfile);
ssize_t lzread(LZFILE *lzfile, void *buf, size_t len);

static LZFILE *lzopen_internal(const char *path, const char *mode, int fd)
{
	int level = 5;
	int encoding = 0;
	FILE *fp;
	LZFILE *lzfile;
	lzma_ret ret;
	lzma_stream lzma_strm_tmp = LZMA_STREAM_INIT;

	for (; *mode; mode++) {
		if (*mode == 'w')
			encoding = 1;
		else if (*mode == 'r')
			encoding = 0;
		else if (*mode >= '1' && *mode <= '9')
			level = *mode - '0';
	}
	if (fd != -1)
		fp = fdopen(fd, encoding ? "w" : "r");
	else
		fp = fopen(path, encoding ? "w" : "r");
	if (!fp)
		return NULL;

	lzfile = calloc(1, sizeof(*lzfile));

	if (!lzfile) {
		fclose(fp);
		return NULL;
	}

	lzfile->file = fp;
	lzfile->encoding = encoding;
	lzfile->eof = 0;
	lzfile->strm = lzma_strm_tmp;
	if (encoding) {
		lzma_options_lzma opt_lzma;
		if (lzma_lzma_preset(&opt_lzma, level - 1))
			return NULL;
		ret = lzma_alone_encoder(&lzfile->strm, &opt_lzma);
	} else {
		ret = lzma_auto_decoder(&lzfile->strm,
					UINT64_C(128) * 1024 * 1024, 0);
	}
	if (ret != LZMA_OK) {
		fclose(fp);
		free(lzfile);
		return NULL;
	}
	return lzfile;
}

LZFILE *lzopen(const char *path, const char *mode)
{
	return lzopen_internal(path, mode, -1);
}

int lzclose(LZFILE *lzfile)
{
	lzma_ret ret;
	size_t n;

	if (!lzfile)
		return -1;

	if (lzfile->encoding) {
		for (;;) {
			lzfile->strm.avail_out = kBufferSize;
			lzfile->strm.next_out = lzfile->buf;
			ret = lzma_code(&lzfile->strm, LZMA_FINISH);
			if (ret != LZMA_OK && ret != LZMA_STREAM_END)
				return -1;
			n = kBufferSize - lzfile->strm.avail_out;
			if (n && fwrite(lzfile->buf, 1, n, lzfile->file) != n)
				return -1;
			if (ret == LZMA_STREAM_END)
				break;
		}
	}
	lzma_end(&lzfile->strm);

	return fclose(lzfile->file);
	free(lzfile);
}

ssize_t lzread(LZFILE *lzfile, void *buf, size_t len)
{
	lzma_ret ret;
	int eof = 0;

	if (!lzfile || lzfile->encoding)
		return -1;

	if (lzfile->eof)
		return 0;

	lzfile->strm.next_out = buf;
	lzfile->strm.avail_out = len;

	for (;;) {
		if (!lzfile->strm.avail_in) {
			lzfile->strm.next_in = lzfile->buf;
			lzfile->strm.avail_in = fread(lzfile->buf, 1, kBufferSize, lzfile->file);
			if (!lzfile->strm.avail_in)
				eof = 1;
		}

		ret = lzma_code(&lzfile->strm, LZMA_RUN);
		if (ret == LZMA_STREAM_END) {
			lzfile->eof = 1;
			return len - lzfile->strm.avail_out;
		}

		if (ret != LZMA_OK)
			return -1;

		if (!lzfile->strm.avail_out)
			return len;

		if (eof)
			return -1;
	}
}

int is_lzma_file(const char *filename)
{
	FILE *fp;
	int ret = 0;
	uint8_t buf[13];

	if (!filename)
		return 0;

	fp = fopen(filename, "r");
	if (fp == NULL)
		return 0;

	const size_t size = fread(buf, 1, sizeof(buf), fp);

	if (size != 13) {
		/* file is too small to be a lzma file. */
		fclose(fp);
		return 0;
	}

	lzma_filter filter = { .id = LZMA_FILTER_LZMA1 };

	switch (lzma_properties_decode(&filter, NULL, buf, 5)) {
	case LZMA_OK:
		ret = 1;
		break;
	default:
		/* It's not a lzma file */
		ret = 0;
	}

	fclose(fp);
	return ret;
}

char *lzma_decompress_file(const char *filename, off_t *r_size)
{
	LZFILE *fp;
	char *buf;
	off_t size, allocated;
	ssize_t result;

	dbgprintf("Try LZMA decompression.\n");

	*r_size = 0;
	if (!filename)
		return NULL;

	if (!is_lzma_file(filename))
		return NULL;

	fp = lzopen(filename, "rb");
	if (fp == 0) {
		dbgprintf("Cannot open `%s'\n", filename);
		return NULL;
	}
	size = 0;
	allocated = 65536;
	buf = xmalloc(allocated);
	do {
		if (size == allocated) {
			allocated <<= 1;
			buf = xrealloc(buf, allocated);
		}
		result = lzread(fp, buf + size, allocated - size);
		if (result < 0) {
			if ((errno == EINTR) || (errno == EAGAIN))
				continue;

			dbgprintf("%s: read on %s of %ld bytes failed\n",
				__func__, filename, (allocated - size) + 0UL);
			break;
		}
		size += result;
	} while (result > 0);

	if (lzclose(fp) != LZMA_OK) {
		dbgprintf("%s: Close of %s failed\n", __func__, filename);
		goto fail;
	}
	if (result < 0)
		goto fail;

	*r_size =  size;
	return buf;
fail:
	free(buf);
	return NULL;
}
#else
char *lzma_decompress_file(const char *UNUSED(filename), off_t *UNUSED(r_size))
{
	return NULL;
}
#endif /* HAVE_LIBLZMA */
