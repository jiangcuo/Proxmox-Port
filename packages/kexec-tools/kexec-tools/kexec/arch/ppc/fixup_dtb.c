#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include <libfdt.h>
#include "ops.h"
#include "page.h"
#include "fixup_dtb.h"
#include "kexec-ppc.h"

const char proc_dts[] = "/proc/device-tree";

static void print_fdt_reserve_regions(char *blob_buf)
{
	int i, num;

	if (!kexec_debug)
		return;
	/* Print out a summary of the final reserve regions */
	num =  fdt_num_mem_rsv(blob_buf);
	dbgprintf ("reserve regions: %d\n", num);
	for (i = 0; i < num; i++) {
		uint64_t offset, size;

		if (fdt_get_mem_rsv(blob_buf, i, &offset, &size) == 0) {
			dbgprintf("%d: offset: %llx, size: %llx\n", i, offset, size);
		} else {
			dbgprintf("Error retreiving reserved region\n");
		}
	}
}


static void fixup_nodes(char *nodes[])
{
	int index = 0;
	char *fname;
	char *prop_name;
	char *node_name;
	void *node;
	int len;
	char *content;
	off_t content_size;
	int ret;

	while (nodes[index]) {

		len = asprintf(&fname, "%s%s", proc_dts, nodes[index]);
		if (len < 0)
			die("asprintf() failed\n");

		content = slurp_file(fname, &content_size);
		if (!content) {
			die("Can't open %s: %s\n", fname, strerror(errno));
		}

		prop_name = fname + len;
		while (*prop_name != '/')
			prop_name--;

		*prop_name = '\0';
		prop_name++;

		node_name = fname + sizeof(proc_dts) - 1;

		node = finddevice(node_name);
		if (!node)
			node = create_node(NULL, node_name + 1);

		ret = setprop(node, prop_name, content, content_size);
		if (ret < 0)
			die("setprop of %s/%s size: %ld failed: %s\n",
					node_name, prop_name, content_size,
					fdt_strerror(ret));

		free(content);
		free(fname);
		index++;
	};
}

/*
 * command line priority:
 * - use the supplied command line
 * - if none available use the command line from .dtb
 * - if not available use the current command line
 */
static void fixup_cmdline(const char *cmdline)
{
	void *chosen;
	char *fixup_cmd_node[] = {
		"/chosen/bootargs",
		NULL,
	};

	chosen = finddevice("/chosen");

	if (!cmdline) {
		if (!chosen)
			fixup_nodes(fixup_cmd_node);
	} else {
		if (!chosen)
			chosen = create_node(NULL, "chosen");
		setprop_str(chosen, "bootargs", cmdline);
	}
	return;
}

#define EXPAND_GRANULARITY     1024

static char *expand_buf(int minexpand, char *blob_buf, off_t *blob_size)
{
	int size = fdt_totalsize(blob_buf);
	int rc;

	size = _ALIGN(size + minexpand, EXPAND_GRANULARITY);
	blob_buf = realloc(blob_buf, size);
	if (!blob_buf)
		die("Couldn't find %d bytes to expand device tree\n\r", size);
	rc = fdt_open_into(blob_buf, blob_buf, size);
	if (rc != 0)
		die("Couldn't expand fdt into new buffer: %s\n\r",
			fdt_strerror(rc));

	*blob_size = fdt_totalsize(blob_buf);

	return blob_buf;
}

static void fixup_reserve_regions(struct kexec_info *info, char *blob_buf)
{
	int ret, i;
	int nodeoffset;
	u64 val = 0;

	/* If this is a KEXEC kernel we add all regions since they will
	 * all need to be saved */
	if (info->kexec_flags & KEXEC_ON_CRASH) {
		for (i = 0; i < info->nr_segments; i++) {
			uint64_t address = (unsigned long)info->segment[i].mem;
			uint64_t size = info->segment[i].memsz;

			while ((i+1) < info->nr_segments &&
			      (address + size == (unsigned long)info->segment[i+1].mem)) {
				size += info->segment[++i].memsz;
			}

			ret = fdt_add_mem_rsv(blob_buf, address, size);
			if (ret) {
				printf("%s: Error adding memory range to memreserve!\n",
					fdt_strerror(ret));
				goto out;
			}
		}
	} else if (ramdisk || reuse_initrd) {
		/* Otherwise we just add back the ramdisk and the device tree
		 * is already in the list */
		ret = fdt_add_mem_rsv(blob_buf, ramdisk_base, ramdisk_size);
		if (ret) {
			printf("%s: Unable to add new reserved memory for initrd flat device tree\n",
				fdt_strerror(ret));
			goto out;
		}
	}

#if 0
	/* XXX: Do not reserve spin-table for CPUs. */

	/* Add reserve regions for cpu-release-addr */
	nodeoffset = fdt_node_offset_by_prop_value(blob_buf, -1, "device_type", "cpu", 4);
	while (nodeoffset != -FDT_ERR_NOTFOUND) {
		const void *buf;
		int sz, ret;
		u64 tmp;

		buf = fdt_getprop(blob_buf, nodeoffset, "cpu-release-addr", &sz);

		if (buf) {
			if (sz == 4) {
				tmp = *(u32 *)buf;
			} else if (sz == 8) {
				tmp = *(u64 *)buf;
			}

			/* crude check to see if last value is repeated */
			if (_ALIGN_DOWN(tmp, PAGE_SIZE) != _ALIGN_DOWN(val, PAGE_SIZE)) {
				val = tmp;
				ret = fdt_add_mem_rsv(blob_buf, _ALIGN_DOWN(val, PAGE_SIZE), PAGE_SIZE);
				if (ret)
					printf("%s: Unable to add reserve for cpu-release-addr!\n",
						fdt_strerror(ret));
			}
		}

		nodeoffset = fdt_node_offset_by_prop_value(blob_buf, nodeoffset,
				"device_type", "cpu", 4);
	}
#endif

out:
	print_fdt_reserve_regions(blob_buf);
}

static void fixup_memory(struct kexec_info *info, char *blob_buf)
{
	if (info->kexec_flags & KEXEC_ON_CRASH) {
		int nodeoffset, len = 0;
		u8 tmp[16];
		const unsigned long *addrcell, *sizecell;

		nodeoffset = fdt_path_offset(blob_buf, "/memory");

		if (nodeoffset < 0) {
			printf("Error searching for memory node!\n");
			return;
		}

		addrcell = fdt_getprop(blob_buf, 0, "#address-cells", NULL);
		/* use shifts and mask to ensure endianness */
		if ((addrcell) && (*addrcell == 2)) {
			tmp[0] = (crash_base >> 56) & 0xff;
			tmp[1] = (crash_base >> 48) & 0xff;
			tmp[2] = (crash_base >> 40) & 0xff;
			tmp[3] = (crash_base >> 32) & 0xff;
			tmp[4] = (crash_base >> 24) & 0xff;
			tmp[5] = (crash_base >> 16) & 0xff;
			tmp[6] = (crash_base >>  8) & 0xff;
			tmp[7] = (crash_base      ) & 0xff;
			len = 8;
		} else {
			tmp[0] = (crash_base >> 24) & 0xff;
			tmp[1] = (crash_base >> 16) & 0xff;
			tmp[2] = (crash_base >>  8) & 0xff;
			tmp[3] = (crash_base      ) & 0xff;
			len = 4;
		}

		sizecell = fdt_getprop(blob_buf, 0, "#size-cells", NULL);
		/* use shifts and mask to ensure endianness */
		if ((sizecell) && (*sizecell == 2)) {
			tmp[0+len] = (crash_size >> 56) & 0xff;
			tmp[1+len] = (crash_size >> 48) & 0xff;
			tmp[2+len] = (crash_size >> 40) & 0xff;
			tmp[3+len] = (crash_size >> 32) & 0xff;
			tmp[4+len] = (crash_size >> 24) & 0xff;
			tmp[5+len] = (crash_size >> 16) & 0xff;
			tmp[6+len] = (crash_size >>  8) & 0xff;
			tmp[7+len] = (crash_size      ) & 0xff;
			len += 8;
		} else {
			tmp[0+len] = (crash_size >> 24) & 0xff;
			tmp[1+len] = (crash_size >> 16) & 0xff;
			tmp[2+len] = (crash_size >>  8) & 0xff;
			tmp[3+len] = (crash_size      ) & 0xff;
			len += 4;
		}

		if (fdt_setprop(blob_buf, nodeoffset, "reg", tmp, len) != 0) {
			printf ("Error setting memory node!\n");
		}

		fdt_delprop(blob_buf, nodeoffset, "linux,usable-memory");
	}
}

/* removes crashkernel nodes if they exist and we are *rebooting*
 * into a crashkernel. These nodes should not exist after we
 * crash and reboot into a new kernel
 */
static void fixup_crashkernel(struct kexec_info *info, char *blob_buf)
{
	int nodeoffset;

	nodeoffset = fdt_path_offset(blob_buf, "/chosen");

	if (info->kexec_flags & KEXEC_ON_CRASH) {
		if (nodeoffset < 0) {
			printf("fdt_crashkernel: %s\n", fdt_strerror(nodeoffset));
			return;
		}

		fdt_delprop(blob_buf, nodeoffset, "linux,crashkernel-base");
		fdt_delprop(blob_buf, nodeoffset, "linux,crashkernel-size");
	}
}
/* remove the old chosen nodes if they exist and add correct chosen
 * nodes if we have an initd
 */
static void fixup_initrd(char *blob_buf)
{
	int err, nodeoffset;
	unsigned long tmp;

	nodeoffset = fdt_path_offset(blob_buf, "/chosen");

	if (nodeoffset < 0) {
		printf("fdt_initrd: %s\n", fdt_strerror(nodeoffset));
		return;
	}

	fdt_delprop(blob_buf, nodeoffset, "linux,initrd-start");
	fdt_delprop(blob_buf, nodeoffset, "linux,initrd-end");

	if ((reuse_initrd || ramdisk) &&
	   ((ramdisk_base != 0) && (ramdisk_size != 0))) {
		tmp = ramdisk_base;
		err = fdt_setprop(blob_buf, nodeoffset,
			"linux,initrd-start", &tmp, sizeof(tmp));
		if (err < 0) {
			printf("WARNING: "
				"could not set linux,initrd-start %s.\n",
				fdt_strerror(err));
				return;
		}

		tmp = ramdisk_base + ramdisk_size;
		err = fdt_setprop(blob_buf, nodeoffset,
			"linux,initrd-end", &tmp, sizeof(tmp));
		if (err < 0) {
			printf("WARNING: could not set linux,initrd-end %s.\n",
				fdt_strerror(err));
				return;
		}
	}
}

char *fixup_dtb_init(struct kexec_info *info, char *blob_buf, off_t *blob_size,
			unsigned long hole_addr, unsigned long *dtb_addr)
{
	int ret, i, num = fdt_num_mem_rsv(blob_buf);

	fdt_init(blob_buf);

	/* Remove the existing reserve regions as they will no longer
	 * be valid after we reboot */
	for (i = num - 1; i >= 0; i--) {
		ret = fdt_del_mem_rsv(blob_buf, i);
		if (ret) {
			printf("%s: Error deleting memory reserve region %d from device tree!\n",
					fdt_strerror(ret), i);
		}
	}

	/* Pack the FDT first, so we don't grow excessively if there is already free space */
	ret = fdt_pack(blob_buf);
	if (ret)
		printf("%s: Unable to pack flat device tree\n", fdt_strerror(ret));

	/* info->nr_segments just a guide, will grow by at least EXPAND_GRANULARITY */
	blob_buf = expand_buf(info->nr_segments * sizeof(struct fdt_reserve_entry),
				 blob_buf, blob_size);

	/* add reserve region for *THIS* fdt */
	*dtb_addr = locate_hole(info, *blob_size, 0,
				hole_addr, hole_addr+KERNEL_ACCESS_TOP, -1);
	ret = fdt_add_mem_rsv(blob_buf, *dtb_addr, PAGE_ALIGN(*blob_size));
	if (ret) {
		printf("%s: Unable to add new reserved memory for the flat device tree\n",
			fdt_strerror(ret));
	}

	return blob_buf;
}

static void save_fixed_up_dtb(char *blob_buf, off_t blob_size)
{
	FILE *fp;

	if (!kexec_debug)
		return;
	fp = fopen("debug.dtb", "w");
	if (fp) {
		if ( blob_size == fwrite(blob_buf, sizeof(char), blob_size, fp)) {
			dbgprintf("debug.dtb written\n");
		} else {
			dbgprintf("Unable to write debug.dtb\n");
		}

		fclose(fp);
	} else {
		dbgprintf("Unable to dump flat device tree to debug.dtb\n");
	}
}

char *fixup_dtb_finalize(struct kexec_info *info, char *blob_buf, off_t *blob_size,
			char *nodes[], char *cmdline)
{
	fixup_nodes(nodes);
	fixup_cmdline(cmdline);
	fixup_reserve_regions(info, blob_buf);
	fixup_memory(info, blob_buf);
	fixup_initrd(blob_buf);
	fixup_crashkernel(info, blob_buf);

	blob_buf = (char *)dt_ops.finalize();
	*blob_size = fdt_totalsize(blob_buf);

	save_fixed_up_dtb(blob_buf, *blob_size);

	return blob_buf;
}
