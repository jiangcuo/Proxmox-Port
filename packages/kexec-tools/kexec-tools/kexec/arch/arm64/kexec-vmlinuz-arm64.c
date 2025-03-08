/*
 * ARM64 PE compressed Image (vmlinuz, ZBOOT) support.
 *
 * Several distros use 'make zinstall' rule inside
 * 'arch/arm64/boot/Makefile' to install the arm64
 * ZBOOT compressed file inside the boot destination
 * directory (for e.g. /boot).
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
 * Note this, module doesn't provide a _load() function instead
 * relying on image_arm64_load() to load the resulting decompressed
 * image.
 *
 * So basically the kernel space still gets a decompressed
 * kernel image to load via kexec-tools.
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "kexec-arm64.h"
#include <kexec-pe-zboot.h>
#include "arch/options.h"

static int kernel_fd = -1;
static off_t decompressed_size;

/* Returns:
 * -1 : in case of error/invalid format (not a valid PE+compressed ZBOOT format.
 */
int pez_arm64_probe(const char *kernel_buf, off_t kernel_size)
{
	int ret = -1;
	const struct arm64_image_header *h;
	char *buf;
	off_t buf_sz;

	buf = (char *)kernel_buf;
	buf_sz = kernel_size;
	if (!buf)
		return -1;
	h = (const struct arm64_image_header *)buf;

	dbgprintf("%s: PROBE.\n", __func__);
	if (buf_sz < sizeof(struct arm64_image_header)) {
		dbgprintf("%s: Not large enough to be a PE image.\n", __func__);
		return -1;
	}
	if (!arm64_header_check_pe_sig(h)) {
		dbgprintf("%s: Not an PE image.\n", __func__);
		return -1;
	}

	if (buf_sz < sizeof(struct arm64_image_header) + h->pe_header) {
		dbgprintf("%s: PE image offset larger than image.\n", __func__);
		return -1;
	}

	if (memcmp(&buf[h->pe_header],
		   arm64_pe_machtype, sizeof(arm64_pe_machtype))) {
		dbgprintf("%s: PE header doesn't match machine type.\n", __func__);
		return -1;
	}

	ret = pez_prepare(buf, buf_sz, &kernel_fd, &decompressed_size);

	if (!ret) {
	    /* validate the arm64 specific header */
	    struct arm64_image_header hdr_check;
	    if (read(kernel_fd, &hdr_check, sizeof(hdr_check)) != sizeof(hdr_check))
		goto bad_header;

	    lseek(kernel_fd, 0, SEEK_SET);

	    if (!arm64_header_check_magic(&hdr_check)) {
		dbgprintf("%s: Bad arm64 image header.\n", __func__);
		goto bad_header;
	    }
	}

	return ret;
bad_header:
	close(kernel_fd);
	free(buf);
	return -1;
}

int pez_arm64_load(int argc, char **argv, const char *buf, off_t len,
			struct kexec_info *info)
{
	if (kernel_fd > 0 && decompressed_size > 0) {
		char *kbuf;
		off_t nread;
		int fd;

		info->kernel_fd = kernel_fd;
		fd = dup(kernel_fd);
		if (fd < 0) {
			dbgprintf("%s: dup fd failed.\n", __func__);
			return -1;
		}
		kbuf = slurp_fd(fd, NULL, decompressed_size, &nread);
		if (!kbuf || nread != decompressed_size) {
			dbgprintf("%s: slurp_fd failed.\n", __func__);
			return -1;
		}
		return image_arm64_load(argc, argv, kbuf, decompressed_size, info);
	}

	dbgprintf("%s: wrong kernel file descriptor.\n", __func__);
	return -1;
}

void pez_arm64_usage(void)
{
	printf(
"     An ARM64 vmlinuz, PE image of a compressed, little endian.\n"
"     kernel, built with ZBOOT enabled.\n\n");
}
