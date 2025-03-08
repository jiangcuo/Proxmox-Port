/*
 * Generic PE compressed Image (vmlinuz, ZBOOT) support.
 *
 * Several distros use 'make zinstall' with CONFIG_ZBOOT
 * enabled to create UEFI PE images that contain
 * a decompressor and a compressed kernel image.
 *
 * Currently we cannot use kexec_file_load() to load vmlinuz
 * PE images that self decompress.
 *
 * To support ZBOOT, we should:
 * a). Copy the compressed contents of vmlinuz to a temporary file.
 * b). Decompress (gunzip-decompress) the contents inside the
 *     temporary file.
 * c). Validate the resulting image and write it back to the
 *     temporary file.
 * d). Pass the 'fd' of the temporary file to the kernel space.
 *
 * This module contains the arch independent code for the above,
 * arch specific PE and image checks should wrap calls
 * to functions in this module.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "kexec.h"
#include <kexec-pe-zboot.h>

#define FILENAME_IMAGE		"/tmp/ImageXXXXXX"

/*
 * Returns -1 : in case of error/invalid format (not a valid PE+compressed ZBOOT format.
 *
 * crude_buf: the content, which is read from the kernel file without any processing
 */
int pez_prepare(const char *crude_buf, off_t buf_sz, int *kernel_fd,
		off_t *kernel_size)
{
	int ret = -1;
	int fd = 0;
	char *fname = NULL;
	char *kernel_uncompressed_buf = NULL;
	off_t decompressed_size = 0;
	const struct linux_pe_zboot_header *z;

	z = (const struct linux_pe_zboot_header *)(crude_buf);

	if (memcmp(&z->image_type, "zimg", sizeof(z->image_type))) {
		dbgprintf("%s: PE doesn't contain a compressed kernel.\n", __func__);
		return -1;
	}

	/*
	 * At the moment its possible to create images with more compression
	 * algorithms than are supported here, error out if we detect that.
	 */
	if (memcmp(&z->compress_type, "gzip", 4) &&
	    memcmp(&z->compress_type, "lzma", 4)) {
		dbgprintf("%s: kexec can only decompress gziped and lzma images.\n", __func__);
		return -1;
	}

	if (buf_sz < z->payload_offset + z->payload_size) {
		dbgprintf("%s: PE too small to contain complete payload.\n", __func__);
		return -1;
	}

	if (!(fname = strdup(FILENAME_IMAGE))) {
		dbgprintf("%s: Can't duplicate strings\n", __func__);
		return -1;
	}

	if ((fd = mkstemp(fname)) < 0) {
		dbgprintf("%s: Can't open file %s\n", __func__,	fname);
		ret = -1;
		goto fail_mkstemp;
	}

	if (write(fd, &crude_buf[z->payload_offset],
		  z->payload_size) != z->payload_size) {
		dbgprintf("%s: Can't write the compressed file %s\n",
				__func__, fname);
		ret = -1;
		goto fail_write;
	}

	kernel_uncompressed_buf = slurp_decompress_file(fname,
							&decompressed_size);

	dbgprintf("%s: decompressed size %ld\n", __func__, decompressed_size);

	lseek(fd, 0, SEEK_SET);

	if (write(fd,  kernel_uncompressed_buf,
		  decompressed_size) != decompressed_size) {
		dbgprintf("%s: Can't write the decompressed file %s\n",
				__func__, fname);
		ret = -1;
		goto fail_bad_header;
	}

	*kernel_fd = open(fname, O_RDONLY);
	if (*kernel_fd == -1) {
		dbgprintf("%s: Failed to open file %s\n",
				__func__, fname);
		ret = -1;
		goto fail_bad_header;
	}

	*kernel_size = decompressed_size;
	dbgprintf("%s: done\n", __func__);

	ret = 0;
	goto fail_write;

fail_bad_header:
	free(kernel_uncompressed_buf);

fail_write:
	if (fd >= 0)
		close(fd);

	unlink(fname);

fail_mkstemp:
	free(fname);

	return ret;
}
