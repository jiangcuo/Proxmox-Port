/*
 * LoongArch PE compressed Image (vmlinuz, ZBOOT) support.
 * Based on arm64 code
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "kexec.h"
#include "kexec-loongarch.h"
#include <kexec-pe-zboot.h>
#include "arch/options.h"

static int kernel_fd = -1;
static off_t decompressed_size;

/* Returns:
 * -1 : in case of error/invalid format (not a valid PE+compressed ZBOOT format.
 */
int pez_loongarch_probe(const char *kernel_buf, off_t kernel_size)
{
	int ret = -1;
	const struct loongarch_image_header *h;
	char *buf;
	off_t buf_sz;

	buf = (char *)kernel_buf;
	buf_sz = kernel_size;
	if (!buf)
		return -1;
	h = (const struct loongarch_image_header *)buf;

	dbgprintf("%s: PROBE.\n", __func__);
	if (buf_sz < sizeof(struct loongarch_image_header)) {
		dbgprintf("%s: Not large enough to be a PE image.\n", __func__);
		return -1;
	}
	if (!loongarch_header_check_pe_sig(h)) {
		dbgprintf("%s: Not an PE image.\n", __func__);
		return -1;
	}

	if (buf_sz < sizeof(struct loongarch_image_header) + h->pe_header) {
		dbgprintf("%s: PE image offset larger than image.\n", __func__);
		return -1;
	}

	if (memcmp(&buf[h->pe_header],
		   loongarch_pe_machtype, sizeof(loongarch_pe_machtype))) {
		dbgprintf("%s: PE header doesn't match machine type.\n", __func__);
		return -1;
	}

	ret = pez_prepare(buf, buf_sz, &kernel_fd, &decompressed_size);

	/* Fixme: add sanity check of the decompressed kernel before return */
	return ret;
}

int pez_loongarch_load(int argc, char **argv, const char *buf, off_t len,
			struct kexec_info *info)
{
	if (kernel_fd > 0 && decompressed_size > 0) {
		char *kbuf;
		off_t nread;

		info->kernel_fd = kernel_fd;
		/*
		 * slurp_fd will close kernel_fd, but it is safe here
		 * due to no kexec_file_load support.
		 */
		kbuf = slurp_fd(kernel_fd, NULL, decompressed_size, &nread);
		if (!kbuf || nread != decompressed_size) {
			dbgprintf("%s: slurp_fd failed.\n", __func__);
			return -1;
		}
		return pei_loongarch_load(argc, argv, kbuf, decompressed_size, info);
	}

	dbgprintf("%s: wrong kernel file descriptor.\n", __func__);
	return -1;
}

void pez_loongarch_usage(void)
{
	printf(
"     An LoongArch vmlinuz, PE image of a compressed, little endian.\n"
"     kernel, built with ZBOOT enabled.\n\n");
}
