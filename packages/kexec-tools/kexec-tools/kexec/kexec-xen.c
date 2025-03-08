#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <elf.h>
#include "kexec.h"
#include "kexec-syscall.h"
#include "crashdump.h"

#include "config.h"

#ifdef HAVE_LIBXENCTRL
#include "kexec-xen.h"

#include "crashdump.h"

#ifdef CONFIG_LIBXENCTRL_DL
#include <dlfcn.h>

/* The handle from dlopen(), needed by dlsym(), dlclose() */
static void *xc_dlhandle;
xc_hypercall_buffer_t XC__HYPERCALL_BUFFER_NAME(HYPERCALL_BUFFER_NULL);

void *__xc_dlsym(const char *symbol)
{
	return dlsym(xc_dlhandle, symbol);
}

xc_interface *__xc_interface_open(xentoollog_logger *logger,
				  xentoollog_logger *dombuild_logger,
				  unsigned open_flags)
{
	xc_interface *xch = NULL;

	if (!xc_dlhandle)
		xc_dlhandle = dlopen("libxenctrl.so", RTLD_NOW | RTLD_NODELETE);

	if (xc_dlhandle) {
		typedef xc_interface *(*func_t)(xentoollog_logger *logger,
			xentoollog_logger *dombuild_logger,
			unsigned open_flags);

		func_t func = (func_t)dlsym(xc_dlhandle, "xc_interface_open");
		xch = func(logger, dombuild_logger, open_flags);
	}

	return xch;
}

int __xc_interface_close(xc_interface *xch)
{
	int rc = -1;

	if (xc_dlhandle) {
		typedef int (*func_t)(xc_interface *xch);

		func_t func = (func_t)dlsym(xc_dlhandle, "xc_interface_close");
		rc = func(xch);
		dlclose(xc_dlhandle);
		xc_dlhandle = NULL;
	}

	return rc;
}
#endif /* CONFIG_LIBXENCTRL_DL */

int xen_get_kexec_range(int range, uint64_t *start, uint64_t *end)
{
	uint64_t size;
	xc_interface *xc;
	int rc = -1;

	xc = xc_interface_open(NULL, NULL, 0);
	if (!xc) {
		fprintf(stderr, "failed to open xen control interface.\n");
		goto out;
	}

	rc = xc_kexec_get_range(xc, range, 0, &size, start);
	if (rc < 0) {
		fprintf(stderr, "failed to get range=%d from hypervisor.\n", range);
		goto out_close;
	}

	*end = *start + size - 1;

out_close:
	xc_interface_close(xc);

out:
	return rc;
}

static uint8_t xen_get_kexec_type(unsigned long kexec_flags)
{
	if (kexec_flags & KEXEC_ON_CRASH)
		return KEXEC_TYPE_CRASH;

	if (kexec_flags & KEXEC_LIVE_UPDATE)
		return KEXEC_TYPE_LIVE_UPDATE;

	return KEXEC_TYPE_DEFAULT;
}

#define IDENTMAP_1MiB (1024 * 1024)

int xen_kexec_load(struct kexec_info *info)
{
	uint32_t nr_segments = info->nr_segments, nr_low_segments = 0;
	struct kexec_segment *segments = info->segment;
	uint64_t low_watermark = 0;
	xc_interface *xch;
	xc_hypercall_buffer_array_t *array = NULL;
	uint8_t type;
	uint8_t arch;
	xen_kexec_segment_t *xen_segs, *seg;
	int s;
	int ret = -1;

	xch = xc_interface_open(NULL, NULL, 0);
	if (!xch)
		return -1;

	/*
	 * Ensure 0 - 1 MiB is mapped and accessible by the image.
	 * This allows access to the VGA memory and the region
	 * purgatory copies in the crash case.
	 *
	 * First, count the number of additional segments which will
	 * need to be added in between the ones in segments[].
	 *
	 * The segments are already sorted.
	 */
	for (s = 0; s < nr_segments && (uint64_t)segments[s].mem <= IDENTMAP_1MiB; s++) {
		if ((uint64_t)segments[s].mem > low_watermark)
			nr_low_segments++;

		low_watermark = (uint64_t)segments[s].mem + segments[s].memsz;
	}
	if (low_watermark < IDENTMAP_1MiB)
		nr_low_segments++;

	low_watermark = 0;

	xen_segs = calloc(nr_segments + nr_low_segments, sizeof(*xen_segs));
	if (!xen_segs)
		goto out;

	array = xc_hypercall_buffer_array_create(xch, nr_segments);
	if (array == NULL)
		goto out;

	seg = xen_segs;
	for (s = 0; s < nr_segments; s++) {
		DECLARE_HYPERCALL_BUFFER(void, seg_buf);

		if (low_watermark < IDENTMAP_1MiB && (uint64_t)segments[s].mem > low_watermark) {
			set_xen_guest_handle(seg->buf.h, HYPERCALL_BUFFER_NULL);
			seg->buf_size = 0;
			seg->dest_maddr = low_watermark;
			low_watermark = (uint64_t)segments[s].mem;
			if (low_watermark > IDENTMAP_1MiB)
				low_watermark = IDENTMAP_1MiB;
			seg->dest_size = low_watermark - seg->dest_maddr;
			seg++;
		}

		seg_buf = xc_hypercall_buffer_array_alloc(xch, array, s,
							  seg_buf, segments[s].bufsz);
		if (seg_buf == NULL)
			goto out;
		memcpy(seg_buf, segments[s].buf, segments[s].bufsz);

		set_xen_guest_handle(seg->buf.h, seg_buf);
		seg->buf_size = segments[s].bufsz;
		seg->dest_maddr = (uint64_t)segments[s].mem;
		seg->dest_size = segments[s].memsz;
		seg++;

		low_watermark = (uint64_t)segments[s].mem + segments[s].memsz;
	}

	if ((uint64_t)low_watermark < IDENTMAP_1MiB) {
		set_xen_guest_handle(seg->buf.h, HYPERCALL_BUFFER_NULL);
		seg->buf_size = 0;
		seg->dest_maddr = low_watermark;
		seg->dest_size = IDENTMAP_1MiB - low_watermark;
		seg++;
	}

	type = xen_get_kexec_type(info->kexec_flags);

	arch = (info->kexec_flags & KEXEC_ARCH_MASK) >> 16;
#if defined(__i386__) || defined(__x86_64__)
	if (!arch)
		arch = EM_386;
#endif

	ret = xc_kexec_load(xch, type, arch, (uint64_t)info->entry,
			    nr_segments + nr_low_segments, xen_segs);

out:
	xc_hypercall_buffer_array_destroy(xch, array);
	free(xen_segs);
	xc_interface_close(xch);

	return ret;
}

int xen_kexec_unload(uint64_t kexec_flags)
{
	xc_interface *xch;
	uint8_t type;
	int ret;

	xch = xc_interface_open(NULL, NULL, 0);
	if (!xch)
		return -1;

	type = xen_get_kexec_type(kexec_flags);

	ret = xc_kexec_unload(xch, type);

	xc_interface_close(xch);

	return ret;
}

int xen_kexec_status(uint64_t kexec_flags)
{
	xc_interface *xch;
	uint8_t type;
	int ret = -1;

#ifdef HAVE_KEXEC_CMD_STATUS
	xch = xc_interface_open(NULL, NULL, 0);
	if (!xch)
		return -1;

	type = xen_get_kexec_type(kexec_flags);

	ret = xc_kexec_status(xch, type);

	xc_interface_close(xch);
#endif

	return ret;
}

int xen_kexec_exec(uint64_t kexec_flags)
{
	xc_interface *xch;
	uint8_t type = KEXEC_TYPE_DEFAULT;
	int ret;

	xch = xc_interface_open(NULL, NULL, 0);
	if (!xch)
		return -1;

	if (kexec_flags & KEXEC_LIVE_UPDATE)
		type = KEXEC_TYPE_LIVE_UPDATE;

	ret = xc_kexec_exec(xch, type);

	xc_interface_close(xch);

	return ret;
}

#else /* ! HAVE_LIBXENCTRL */

int xen_get_kexec_range(int range, uint64_t *start, uint64_t *end)
{
	return -1;
}

int xen_kexec_load(struct kexec_info *UNUSED(info))
{
	return -1;
}

int xen_kexec_unload(uint64_t kexec_flags)
{
	return -1;
}

int xen_kexec_status(uint64_t kexec_flags)
{
	return -1;
}

int xen_kexec_exec(uint64_t kexec_flags)
{
	return -1;
}

#endif
