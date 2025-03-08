/*
 * uImage support added by Marc Andre Tanner <mat@brain-dump.org>
 *
 * Cloned from ARM by Paul Mundt, 2009.
 */
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <image.h>
#include <kexec-uImage.h>
#include "../../kexec.h"
#include "kexec-sh.h"

int uImage_sh_probe(const char *buf, off_t len)
{
	return uImage_probe_kernel(buf, len, IH_ARCH_SH);
}

int uImage_sh_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info)
{
	return zImage_sh_load(argc, argv, buf + sizeof(struct image_header),
			      len - sizeof(struct image_header), info);
}
