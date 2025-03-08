/*
 * - 08/21/2007 ATAG support added by Uli Luckas <u.luckas@road.de>
 *
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <unistd.h>
#include <libfdt.h>
#include <arch/options.h>
#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "kexec-arm.h"
#include "../../fs2dt.h"
#include "crashdump-arm.h"
#include "iomem.h"

#define BOOT_PARAMS_SIZE 1536

off_t initrd_base, initrd_size;
unsigned int kexec_arm_image_size = 0;
unsigned long long user_page_offset = (-1ULL);

struct zimage_header {
	uint32_t instr[9];
	uint32_t magic;
#define ZIMAGE_MAGIC cpu_to_le32(0x016f2818)
	uint32_t start;
	uint32_t end;
	uint32_t endian;

	/* Extension to the data passed to the boot agent.  The offset
	 * points at a tagged table following a similar format to the
	 * ATAGs.
	 */
	uint32_t magic2;
#define ZIMAGE_MAGIC2 (0x45454545)
	uint32_t extension_tag_offset;
};

struct android_image {
	char magic[8];
	uint32_t kernel_size;
	uint32_t kernel_addr;
	uint32_t ramdisk_size;
	uint32_t ramdisk_addr;
	uint32_t stage2_size;
	uint32_t stage2_addr;
	uint32_t tags_addr;
	uint32_t page_size;
	uint32_t reserved1;
	uint32_t reserved2;
	char name[16];
	char command_line[512];
	uint32_t chksum[8];
};

struct tag_header {
	uint32_t size;
	uint32_t tag;
};

/* The list must start with an ATAG_CORE node */
#define ATAG_CORE       0x54410001

struct tag_core {
	uint32_t flags;	    /* bit 0 = read-only */
	uint32_t pagesize;
	uint32_t rootdev;
};

/* it is allowed to have multiple ATAG_MEM nodes */
#define ATAG_MEM	0x54410002

struct tag_mem32 {
	uint32_t   size;
	uint32_t   start;  /* physical start address */
};

/* describes where the compressed ramdisk image lives (virtual address) */
/*
 * this one accidentally used virtual addresses - as such,
 * it's deprecated.
 */
#define ATAG_INITRD     0x54410005

/* describes where the compressed ramdisk image lives (physical address) */
#define ATAG_INITRD2    0x54420005

struct tag_initrd {
        uint32_t start;    /* physical start address */
        uint32_t size;     /* size of compressed ramdisk image in bytes */
};

/* command line: \0 terminated string */
#define ATAG_CMDLINE    0x54410009

struct tag_cmdline {
	char    cmdline[1];     /* this is the minimum size */
};

/* The list ends with an ATAG_NONE node. */
#define ATAG_NONE       0x00000000

struct tag {
	struct tag_header hdr;
	union {
		struct tag_core	 core;
		struct tag_mem32	mem;
		struct tag_initrd       initrd;
		struct tag_cmdline      cmdline;
	} u;
};

#define tag_next(t)     ((struct tag *)((uint32_t *)(t) + (t)->hdr.size))
#define byte_size(t)    ((t)->hdr.size << 2)
#define tag_size(type)  ((sizeof(struct tag_header) + sizeof(struct type) + 3) >> 2)

struct zimage_tag {
	struct tag_header hdr;
	union {
#define ZIMAGE_TAG_KRNL_SIZE cpu_to_le32(0x5a534c4b)
		struct zimage_krnl_size {
			uint32_t size_ptr;
			uint32_t bss_size;
		} krnl_size;
	} u;
};

int zImage_arm_probe(const char *UNUSED(buf), off_t UNUSED(len))
{
	/* 
	 * Only zImage loading is supported. Do not check if
	 * the buffer is valid kernel image
	 */	
	return 0;
}

void zImage_arm_usage(void)
{
	printf(	"     --command-line=STRING Set the kernel command line to STRING.\n"
		"     --append=STRING       Set the kernel command line to STRING.\n"
		"     --initrd=FILE         Use FILE as the kernel's initial ramdisk.\n"
		"     --ramdisk=FILE        Use FILE as the kernel's initial ramdisk.\n"
		"     --dtb=FILE            Use FILE as the fdt blob.\n"
		"     --atags               Use ATAGs instead of device-tree.\n"
		"     --page-offset=PAGE_OFFSET\n"
		"                           Set PAGE_OFFSET of crash dump vmcore\n"
		);
}

static
struct tag * atag_read_tags(void)
{
	static unsigned long buf[BOOT_PARAMS_SIZE];
	const char fn[]= "/proc/atags";
	FILE *fp;
	fp = fopen(fn, "r");
	if (!fp) {
		fprintf(stderr, "Cannot open %s: %s\n", 
			fn, strerror(errno));
		return NULL;
	}

	if (!fread(buf, sizeof(buf[1]), BOOT_PARAMS_SIZE, fp)) {
		fclose(fp);
		return NULL;
	}

	if (ferror(fp)) {
		fprintf(stderr, "Cannot read %s: %s\n",
			fn, strerror(errno));
		fclose(fp);
		return NULL;
	}

	fclose(fp);
	return (struct tag *) buf;
}

static
int create_mem32_tag(struct tag_mem32 *tag_mem32)
{
	const char fn[]= "/proc/device-tree/memory/reg";
	uint32_t tmp[2];
	FILE *fp;

	fp = fopen(fn, "r");
	if (!fp) {
		fprintf(stderr, "Cannot open %s: %m\n", fn);
		return -1;
	}

	if (fread(tmp, sizeof(tmp[0]), 2, fp) != 2) {
		fprintf(stderr, "Short read %s\n", fn);
		fclose(fp);
		return -1;
	}

	if (ferror(fp)) {
		fprintf(stderr, "Cannot read %s: %m\n", fn);
		fclose(fp);
		return -1;
	}

	/* atags_mem32 has base/size fields reversed! */
	tag_mem32->size = be32_to_cpu(tmp[1]);
	tag_mem32->start = be32_to_cpu(tmp[0]);

	fclose(fp);
	return 0;
}

static
int atag_arm_load(struct kexec_info *info, unsigned long base,
	const char *command_line, off_t command_line_len, const char *initrd)
{
	struct tag *saved_tags = atag_read_tags();
	char *buf;
	off_t len;
	struct tag *params;
	
	buf = xmalloc(getpagesize());
	memset(buf, 0xff, getpagesize());
	params = (struct tag *)buf;

	if (saved_tags) {
		// Copy tags
		saved_tags = (struct tag *) saved_tags;
		while(byte_size(saved_tags)) {
			switch (saved_tags->hdr.tag) {
			case ATAG_INITRD:
			case ATAG_INITRD2:
			case ATAG_CMDLINE:
			case ATAG_NONE:
				// skip these tags
				break;
			default:
				// copy all other tags
				memcpy(params, saved_tags, byte_size(saved_tags));
				params = tag_next(params);
			}
			saved_tags = tag_next(saved_tags);
		}
	} else {
		params->hdr.size = 2;
		params->hdr.tag = ATAG_CORE;
		params = tag_next(params);

		if (!create_mem32_tag(&params->u.mem)) {
			params->hdr.size = 4;
			params->hdr.tag = ATAG_MEM;
			params = tag_next(params);
		}
	}

	if (initrd) {
		params->hdr.size = tag_size(tag_initrd);
		params->hdr.tag = ATAG_INITRD2;
		params->u.initrd.start = initrd_base;
		params->u.initrd.size = initrd_size;
		params = tag_next(params);
	}

	if (command_line) {
		params->hdr.size = (sizeof(struct tag_header) + command_line_len + 3) >> 2;
		params->hdr.tag = ATAG_CMDLINE;
		memcpy(params->u.cmdline.cmdline, command_line,
			command_line_len);
		params->u.cmdline.cmdline[command_line_len - 1] = '\0';
		params = tag_next(params);
	}

	params->hdr.size = 0;
	params->hdr.tag = ATAG_NONE;

	len = ((char *)params - buf) + sizeof(struct tag_header);

	add_segment(info, buf, len, base, len);

	return 0;
}

static int setup_dtb_prop(char **bufp, off_t *sizep, int parentoffset,
		const char *node_name, const char *prop_name,
		const void *val, int len)
{
	char *dtb_buf;
	off_t dtb_size;
	int off;
	int prop_len = 0;
	const struct fdt_property *prop;

	if ((bufp == NULL) || (sizep == NULL) || (*bufp == NULL))
		die("Internal error\n");

	dtb_buf = *bufp;
	dtb_size = *sizep;

	/* check if the subnode has already exist */
	off = fdt_subnode_offset(dtb_buf, parentoffset, node_name);
	if (off == -FDT_ERR_NOTFOUND) {
		dtb_size += fdt_node_len(node_name);
		fdt_set_totalsize(dtb_buf, dtb_size);
		dtb_buf = xrealloc(dtb_buf, dtb_size);
		off = fdt_add_subnode(dtb_buf, parentoffset, node_name);
	}

	if (off < 0) {
		fprintf(stderr, "FDT: Error adding %s node.\n", node_name);
		return -1;
	}

	prop = fdt_get_property(dtb_buf, off, prop_name, &prop_len);
	if ((prop == NULL) && (prop_len != -FDT_ERR_NOTFOUND)) {
		die("FDT: fdt_get_property");
	} else if (prop == NULL) {
		/* prop_len == -FDT_ERR_NOTFOUND */
		/* prop doesn't exist */
		dtb_size += fdt_prop_len(prop_name, len);
	} else {
		if (prop_len < len)
			dtb_size += FDT_TAGALIGN(len - prop_len);
	}

	if (fdt_totalsize(dtb_buf) < dtb_size) {
		fdt_set_totalsize(dtb_buf, dtb_size);
		dtb_buf = xrealloc(dtb_buf, dtb_size);
	}

	if (fdt_setprop(dtb_buf, off, prop_name,
				val, len) != 0) {
		fprintf(stderr, "FDT: Error setting %s/%s property.\n",
				node_name, prop_name);
		return -1;
	}
	*bufp = dtb_buf;
	*sizep = dtb_size;
	return 0;
}

static const struct zimage_tag *find_extension_tag(const char *buf, off_t len,
	uint32_t tag_id)
{
	const struct zimage_header *hdr = (const struct zimage_header *)buf;
	const struct zimage_tag *tag;
	uint32_t offset, size;
	uint32_t max = len - sizeof(struct tag_header);

	if (len < sizeof(*hdr) ||
            hdr->magic != ZIMAGE_MAGIC ||
	    hdr->magic2 != ZIMAGE_MAGIC2)
		return NULL;

	for (offset = hdr->extension_tag_offset;
	     (tag = (void *)(buf + offset)) != NULL &&
	      offset < max &&
	      (size = le32_to_cpu(byte_size(tag))) != 0 &&
	      offset + size < len;
	     offset += size) {
		dbgprintf("  offset 0x%08x tag 0x%08x size %u\n",
			  offset, le32_to_cpu(tag->hdr.tag), size);
		if (tag->hdr.tag == tag_id)
			return tag;
	}

	return NULL;
}

static int get_cells_size(void *fdt, uint32_t *address_cells,
			  uint32_t *size_cells)
{
	int nodeoffset;
	const uint32_t *prop = NULL;
	int prop_len;

	/* default values */
	*address_cells = 1;
	*size_cells = 1;

	/* under root node */
	nodeoffset = fdt_path_offset(fdt, "/");
	if (nodeoffset < 0)
		return -1;

	prop = fdt_getprop(fdt, nodeoffset, "#address-cells", &prop_len);
	if (prop) {
		if (prop_len != sizeof(*prop))
			return -1;

		*address_cells = fdt32_to_cpu(*prop);
	}

	prop = fdt_getprop(fdt, nodeoffset, "#size-cells", &prop_len);
	if (prop) {
		if (prop_len != sizeof(*prop))
			return -1;

		*size_cells = fdt32_to_cpu(*prop);
	}

	dbgprintf("%s: #address-cells:%d #size-cells:%d\n", __func__,
		  *address_cells, *size_cells);
	return 0;
}

static bool cells_size_fitted(uint32_t address_cells, uint32_t size_cells,
			      struct memory_range *range)
{
	dbgprintf("%s: %llx-%llx\n", __func__, range->start, range->end);

	/* if *_cells >= 2, cells can hold 64-bit values anyway */
	if ((address_cells == 1) && (range->start >= (1ULL << 32)))
		return false;

	if ((size_cells == 1) &&
	    ((range->end - range->start + 1) >= (1ULL << 32)))
		return false;

	return true;
}

static void fill_property(void *buf, uint64_t val, uint32_t cells)
{
	uint32_t val32;
	int i;

	if (cells == 1) {
		val32 = cpu_to_fdt32((uint32_t)val);
		memcpy(buf, &val32, sizeof(uint32_t));
	} else {
		for (i = 0;
		     i < (cells * sizeof(uint32_t) - sizeof(uint64_t)); i++)
			*(char *)buf++ = 0;

		val = cpu_to_fdt64(val);
		memcpy(buf, &val, sizeof(uint64_t));
	}
}

static int setup_dtb_prop_range(char **bufp, off_t *sizep, int parentoffset,
				const char *node_name, const char *prop_name,
				struct memory_range *range,
				uint32_t address_cells, uint32_t size_cells)
{
	void *buf, *prop;
	size_t buf_size;
	int result;

	buf_size = (address_cells + size_cells) * sizeof(uint32_t);
	prop = buf = xmalloc(buf_size);

	fill_property(prop, range->start, address_cells);
	prop += address_cells * sizeof(uint32_t);

	fill_property(prop, range->end - range->start + 1, size_cells);
	prop += size_cells * sizeof(uint32_t);

	result = setup_dtb_prop(bufp, sizep, parentoffset, node_name,
				prop_name, buf, buf_size);

	free(buf);

	return result;
}

int zImage_arm_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info)
{
	unsigned long page_size = getpagesize();
	unsigned long base, kernel_base;
	unsigned int atag_offset = 0x1000; /* 4k offset from memory start */
	unsigned int extra_size = 0x8000; /* TEXT_OFFSET */
	uint32_t address_cells, size_cells;
	const struct zimage_tag *tag;
	size_t kernel_buf_size;
	size_t kernel_mem_size;
	const char *command_line;
	char *modified_cmdline = NULL;
	off_t command_line_len;
	const char *ramdisk;
	const char *ramdisk_buf;
	int opt;
	int use_atags;
	int result;
	char *dtb_buf;
	off_t dtb_length;
	char *dtb_file;
	off_t dtb_offset;
	char *end;

	/* See options.h -- add any more there, too. */
	static const struct option options[] = {
		KEXEC_ARCH_OPTIONS
		{ "command-line",	1, 0, OPT_APPEND },
		{ "append",		1, 0, OPT_APPEND },
		{ "initrd",		1, 0, OPT_RAMDISK },
		{ "ramdisk",		1, 0, OPT_RAMDISK },
		{ "dtb",		1, 0, OPT_DTB },
		{ "atags",		0, 0, OPT_ATAGS },
		{ "image-size",		1, 0, OPT_IMAGE_SIZE },
		{ "page-offset",	1, 0, OPT_PAGE_OFFSET },
		{ 0, 			0, 0, 0 },
	};
	static const char short_options[] = KEXEC_ARCH_OPT_STR "";

	/*
	 * Parse the command line arguments
	 */
	command_line = 0;
	command_line_len = 0;
	ramdisk = 0;
	ramdisk_buf = 0;
	initrd_size = 0;
	use_atags = 0;
	dtb_file = NULL;
	while((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch(opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX) {
				break;
			}
		case OPT_APPEND:
			command_line = optarg;
			break;
		case OPT_RAMDISK:
			ramdisk = optarg;
			break;
		case OPT_DTB:
			dtb_file = optarg;
			break;
		case OPT_ATAGS:
			use_atags = 1;
			break;
		case OPT_IMAGE_SIZE:
			kexec_arm_image_size = strtoul(optarg, &end, 0);
			break;
		case OPT_PAGE_OFFSET:
			user_page_offset = strtoull(optarg, &end, 0);
			break;
		}
	}

	if (use_atags && dtb_file) {
		fprintf(stderr, "You can only use ATAGs if you don't specify a "
		        "dtb file.\n");
		return -1;
	}

	if (!use_atags && !dtb_file) {
		int f;

		f = have_sysfs_fdt();
		if (f)
			dtb_file = SYSFS_FDT;
	}

	if (command_line) {
		command_line_len = strlen(command_line) + 1;
		if (command_line_len > COMMAND_LINE_SIZE)
			command_line_len = COMMAND_LINE_SIZE;
	}
	if (ramdisk)
		ramdisk_buf = slurp_file_mmap(ramdisk, &initrd_size);

	if (dtb_file)
		dtb_buf = slurp_file(dtb_file, &dtb_length);

	if (len > sizeof(struct zimage_header)) {
		const struct zimage_header *hdr;
		off_t size;

		hdr = (const struct zimage_header *)buf;

		dbgprintf("zImage header: 0x%08x 0x%08x 0x%08x\n",
			  hdr->magic, hdr->start, hdr->end);

		if (hdr->magic == ZIMAGE_MAGIC) {
			size = le32_to_cpu(hdr->end) - le32_to_cpu(hdr->start);

			dbgprintf("zImage size 0x%llx, file size 0x%llx\n",
				  (unsigned long long)size,
				  (unsigned long long)len);

			if (size > len) {
				fprintf(stderr,
					"zImage is truncated - file 0x%llx vs header 0x%llx\n",
					(unsigned long long)len,
					(unsigned long long)size);
				return -1;
			}
			if (size < len)
				len = size;
		}
	}

	/* Handle android images, 2048 is the minimum page size */
	if (len > 2048 && !strncmp(buf, "ANDROID!", 8)) {
		const struct android_image *aimg = (const void *)buf;
		uint32_t page_size = le32_to_cpu(aimg->page_size);
		uint32_t kernel_size = le32_to_cpu(aimg->kernel_size);
		uint32_t ramdisk_size = le32_to_cpu(aimg->ramdisk_size);
		uint32_t stage2_size = le32_to_cpu(aimg->stage2_size);
		off_t aimg_size = page_size + _ALIGN(kernel_size, page_size) +
			_ALIGN(ramdisk_size, page_size) + stage2_size;

		if (len < aimg_size) {
			fprintf(stderr, "Android image size is incorrect\n");
			return -1;
		}

		/* Get the kernel */
		buf = buf + page_size;
		len = kernel_size;

		/* And the ramdisk if none was given on the command line */
		if (!ramdisk && ramdisk_size) {
			initrd_size = ramdisk_size;
			ramdisk_buf = buf + _ALIGN(kernel_size, page_size);
		}

		/* Likewise for the command line */
		if (!command_line && aimg->command_line[0]) {
			command_line = aimg->command_line;
			if (command_line[sizeof(aimg->command_line) - 1])
				command_line_len = sizeof(aimg->command_line);
			else
				command_line_len = strlen(command_line) + 1;
		}
	}

	/*
	 * Save the length of the compressed kernel image w/o the appended DTB.
	 * This will be required later on when the kernel image contained
	 * in the zImage will be loaded into a kernel memory segment.
	 * And we want to load ONLY the compressed kernel image from the zImage
	 * and discard the appended DTB.
	 */
	kernel_buf_size = len;

	/*
	 * Always extend the zImage by four bytes to ensure that an appended
	 * DTB image always sees an initialised value after _edata.
	 */
	kernel_mem_size = len + 4;

	/*
	 * Check for a kernel size extension, and set or validate the
	 * image size.  This is the total space needed to avoid the
	 * boot kernel BSS, so other data (such as initrd) does not get
	 * overwritten.
	 */
	tag = find_extension_tag(buf, len, ZIMAGE_TAG_KRNL_SIZE);

	/*
	 * The zImage length does not include its stack (4k) or its
	 * malloc space (64k).  Include this.
	 */
	len += 0x11000;

	dbgprintf("zImage requires 0x%08llx bytes\n", (unsigned long long)len);

	if (tag) {
		uint32_t *p = (void *)buf + le32_to_cpu(tag->u.krnl_size.size_ptr);
		uint32_t edata_size = le32_to_cpu(get_unaligned(p));
		uint32_t bss_size = le32_to_cpu(tag->u.krnl_size.bss_size);
		uint32_t kernel_size = edata_size + bss_size;

		dbgprintf("Decompressed kernel sizes:\n");
		dbgprintf(" text+data 0x%08lx bss 0x%08lx total 0x%08lx\n",
			  (unsigned long)edata_size,
			  (unsigned long)bss_size,
			  (unsigned long)kernel_size);

		/*
		 * While decompressing, the zImage is placed past _edata
		 * of the decompressed kernel.  Ensure we account for that.
		 */
		if (kernel_size < edata_size + len)
			kernel_size = edata_size + len;

		dbgprintf("Resulting kernel space: 0x%08lx\n",
			  (unsigned long)kernel_size);

		if (kexec_arm_image_size == 0)
			kexec_arm_image_size = kernel_size;
		else if (kexec_arm_image_size < kernel_size) {
			fprintf(stderr,
				"Kernel size is too small, increasing to 0x%lx\n",
				(unsigned long)kernel_size);
			kexec_arm_image_size = kernel_size;
		}
	}

	/*
	 * If the user didn't specify the size of the image, and we don't
	 * have the extension tables, assume the maximum kernel compression
	 * ratio is 4.  Note that we must include space for the compressed
	 * image here as well.
	 */
	if (!kexec_arm_image_size)
		kexec_arm_image_size = len * 5;

	/*
	 * If we are loading a dump capture kernel, we need to update kernel
	 * command line and also add some additional segments.
	 */
	if (info->kexec_flags & KEXEC_ON_CRASH) {
		uint64_t start, end;

		modified_cmdline = xmalloc(COMMAND_LINE_SIZE);
		memset(modified_cmdline, '\0', COMMAND_LINE_SIZE);

		if (command_line) {
			(void) strncpy(modified_cmdline, command_line,
				       COMMAND_LINE_SIZE);
			modified_cmdline[COMMAND_LINE_SIZE - 1] = '\0';
		}

		if (load_crashdump_segments(info, modified_cmdline) < 0) {
			free(modified_cmdline);
			return -1;
		}

		command_line = modified_cmdline;
		command_line_len = strlen(command_line) + 1;

		/*
		 * We put the dump capture kernel at the start of crashkernel
		 * reserved memory.
		 */
		if (parse_iomem_single(CRASH_KERNEL_BOOT, &start, &end) &&
		    parse_iomem_single(CRASH_KERNEL, &start, &end)) {
			/*
			 * No crash kernel memory reserved. We cannot do more
			 * but just bail out.
			 */
			return ENOCRASHKERNEL;
		}
		base = start;
	} else {
		base = locate_hole(info, len + extra_size, 0, 0,
				   ULONG_MAX, INT_MAX);
	}

	if (base == ULONG_MAX)
		return -1;

	kernel_base = base + extra_size;

	/*
	 * Calculate the minimum address of the initrd, which must be
	 * above the memory used by the zImage while it runs.  This
	 * needs to be page-size aligned.
	 */
	initrd_base = kernel_base + _ALIGN(kexec_arm_image_size, page_size);

	dbgprintf("%-6s: address=0x%08lx size=0x%08lx\n", "Kernel",
		  (unsigned long)kernel_base,
		  (unsigned long)kexec_arm_image_size);

	if (ramdisk_buf) {
		/*
		 * Find a hole to place the initrd. The crash kernel use
		 * fixed address, so no check is ok.
		 */
		if (!(info->kexec_flags & KEXEC_ON_CRASH)) {
			initrd_base = locate_hole(info, initrd_size, page_size,
						  initrd_base,
						  ULONG_MAX, INT_MAX);
			if (initrd_base == ULONG_MAX)
				return -1;
		}

		dbgprintf("%-6s: address=0x%08lx size=0x%08lx\n", "Initrd",
			  (unsigned long)initrd_base,
			  (unsigned long)initrd_size);

		add_segment(info, ramdisk_buf, initrd_size, initrd_base,
			    initrd_size);
	}

	if (use_atags) {
		/*
		 * use ATAGs from /proc/atags
		 */
		if (atag_arm_load(info, base + atag_offset,
		                  command_line, command_line_len,
		                  ramdisk_buf) == -1)
			return -1;
	} else {
		/*
		 * Read a user-specified DTB file.
		 */
		if (dtb_file) {
			if (fdt_check_header(dtb_buf) != 0) {
				fprintf(stderr, "Invalid FDT buffer.\n");
				return -1;
			}

			if (command_line) {
				/*
				 *  Error should have been reported so
				 *  directly return -1
				 */
				if (setup_dtb_prop(&dtb_buf, &dtb_length, 0, "chosen",
						"bootargs", command_line,
						strlen(command_line) + 1))
					return -1;
			}
		} else {
			/*
			 * Extract the DTB from /proc/device-tree.
			 */
			create_flatten_tree(&dtb_buf, &dtb_length, command_line);
		}

		/*
		 * Add the initrd parameters to the dtb
		 */
		if (ramdisk_buf) {
			unsigned long start, end;

			start = cpu_to_be32((unsigned long)(initrd_base));
			end = cpu_to_be32((unsigned long)(initrd_base + initrd_size));

			if (setup_dtb_prop(&dtb_buf, &dtb_length, 0, "chosen",
					"linux,initrd-start", &start,
					sizeof(start)))
				return -1;
			if (setup_dtb_prop(&dtb_buf, &dtb_length, 0, "chosen",
					"linux,initrd-end", &end,
					sizeof(end)))
				return -1;
		}

		if (info->kexec_flags & KEXEC_ON_CRASH) {
			/* Determine #address-cells and #size-cells */
			result = get_cells_size(dtb_buf, &address_cells,
						&size_cells);
			if (result) {
				fprintf(stderr, "Cannot determine cells-size.\n");
				return -1;
			}

			if (!cells_size_fitted(address_cells, size_cells,
					       &elfcorehdr_mem)) {
				fprintf(stderr, "elfcorehdr doesn't fit cells-size.\n");
				return -1;
			}

			if (!cells_size_fitted(address_cells, size_cells,
					       &crash_kernel_mem)) {
				fprintf(stderr, "kexec: usable memory range doesn't fit cells-size.\n");
				return -1;
			}

			/* Add linux,elfcorehdr */
			if (setup_dtb_prop_range(&dtb_buf, &dtb_length, 0,
						 "chosen", "linux,elfcorehdr",
						 &elfcorehdr_mem,
						 address_cells, size_cells))
				return -1;

			/* Add linux,usable-memory-range */
			if (setup_dtb_prop_range(&dtb_buf, &dtb_length, 0,
						 "chosen",
						 "linux,usable-memory-range",
						 &crash_kernel_mem,
						 address_cells, size_cells))
				return -1;
		}

		/*
		 * The dtb must also be placed above the memory used by
		 * the zImage.  We don't care about its position wrt the
		 * ramdisk, but we might as well place it after the initrd.
		 * We leave a buffer page between the initrd and the dtb.
		 */
		dtb_offset = initrd_base + initrd_size + page_size;
		dtb_offset = _ALIGN_DOWN(dtb_offset, page_size);

		/*
		 * Find a hole to place the dtb above the initrd.
		 * Crash kernel use fixed address, no check is ok.
		 */
		if (!(info->kexec_flags & KEXEC_ON_CRASH)) {
			dtb_offset = locate_hole(info, dtb_length, page_size,
						 dtb_offset, ULONG_MAX, INT_MAX);
			if (dtb_offset == ULONG_MAX)
				return -1;
		}

		dbgprintf("%-6s: address=0x%08lx size=0x%08lx\n", "DT",
			  (unsigned long)dtb_offset, (unsigned long)dtb_length);

		add_segment(info, dtb_buf, dtb_length, dtb_offset, dtb_length);
	}

	add_segment(info, buf, kernel_buf_size, kernel_base, kernel_mem_size);

	info->entry = (void*)kernel_base;

	return 0;
}
