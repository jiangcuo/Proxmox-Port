/*
 * uImage support for PowerPC
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <image.h>
#include <getopt.h>
#include <arch/options.h>
#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "kexec-ppc.h"
#include "fixup_dtb.h"
#include <kexec-uImage.h>
#include "crashdump-powerpc.h"
#include <limits.h>

int create_flatten_tree(struct kexec_info *, unsigned char **, unsigned long *,
			char *);

/* See options.h -- add any more there, too. */
static const struct option options[] = {
	KEXEC_ARCH_OPTIONS
	{"command-line",	1, 0, OPT_APPEND},
	{"append",	1, 0, OPT_APPEND},
	{"ramdisk",	1, 0, OPT_RAMDISK},
	{"initrd",	1, 0, OPT_RAMDISK},
	{"dtb",		1, 0, OPT_DTB},
	{"reuse-node",	1, 0, OPT_NODES},
	{0, 0, 0, 0},
};
static const char short_options[] = KEXEC_ARCH_OPT_STR;

void uImage_ppc_usage(void)
{
	printf(
			"    --command-line=STRING Set the kernel command line to STRING.\n"
			"    --append=STRING       Set the kernel command line to STRING.\n"
			"    --ramdisk=<filename>  Initial RAM disk.\n"
			"    --initrd=<filename>   same as --ramdisk\n"
			"    --dtb=<filename>      Specify device tree blob file.\n"
			"    --reuse-node=node     Specify nodes which should be taken from /proc/device-tree.\n"
			"                          Can be set multiple times.\n"
	);
}

/*
 * Load the ramdisk into buffer.
 *  If the supplied image is in uImage format use
 *  uImage_load() to read the payload from the image.
 */
char *slurp_ramdisk_ppc(const char *filename, off_t *r_size)
{
	struct Image_info img;
	off_t size;
	const char *buf = slurp_file(filename, &size);
	int rc;

	/* Check if this is a uImage RAMDisk */
	if (!buf)
		return buf;
	rc = uImage_probe_ramdisk(buf, size, IH_ARCH_PPC); 
	if (rc < 0)
		die("uImage: Corrupted ramdisk file %s\n", filename);
	else if (rc == 0) {
		if (uImage_load(buf, size, &img) != 0)
			die("uImage: Reading %ld bytes from %s failed\n",
				size, filename);
		buf = img.buf;
		size = img.len;
	}

	*r_size = size;
	return buf;
}
	
int uImage_ppc_probe(const char *buf, off_t len)
{
	return uImage_probe_kernel(buf, len, IH_ARCH_PPC);
}

static int ppc_load_bare_bits(int argc, char **argv, const char *buf,
		off_t len, struct kexec_info *info, unsigned int load_addr,
		unsigned int ep)
{
	char *command_line, *cmdline_buf, *crash_cmdline;
	char *tmp_cmdline;
	int command_line_len, crash_cmdline_len;
	char *dtb;
	unsigned int addr;
	unsigned long dtb_addr;
	unsigned long dtb_addr_actual;
#define FIXUP_ENTRYS    (20)
	char *fixup_nodes[FIXUP_ENTRYS + 1];
	int cur_fixup = 0;
	int opt;
	int ret = 0;
	char *seg_buf = NULL;
	off_t seg_size = 0;
	unsigned long long hole_addr;
	unsigned long max_addr;
	char *blob_buf = NULL;
	off_t blob_size = 0;
	char *error_msg = NULL;

	cmdline_buf = NULL;
	command_line = NULL;
	tmp_cmdline = NULL;
	dtb = NULL;
	max_addr = LONG_MAX;

	while ((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch (opt) {
		default:
			/* Ignore core options */
			if (opt < OPT_ARCH_MAX) {
				break;
			}
		case OPT_APPEND:
			tmp_cmdline = optarg;
			break;

		case OPT_RAMDISK:
			ramdisk = optarg;
			break;

		case OPT_DTB:
			dtb = optarg;
			break;

		case OPT_NODES:
			if (cur_fixup >= FIXUP_ENTRYS) {
				die("The number of entries for the fixup is too large\n");
			}
			fixup_nodes[cur_fixup] = optarg;
			cur_fixup++;
			break;
		}
	}

	if (ramdisk && reuse_initrd)
		die("Can't specify --ramdisk or --initrd with --reuseinitrd\n");

	command_line_len = 0;
	if (tmp_cmdline) {
		command_line = tmp_cmdline;
	} else {
		command_line = get_command_line();
	}
	command_line_len = strlen(command_line) + 1;

	fixup_nodes[cur_fixup] = NULL;

	/*
	 * len contains the length of the whole kernel image except the bss
	 * section. The 1 MiB should cover it. The purgatory and the dtb are
	 * allocated from memtop down towards zero so we should never get too
	 * close to the bss :)
	 */
#define _1MiB	(1 * 1024 * 1024)

	/*
	 * If the provided load_addr cannot be allocated, find a new
	 * area. Rebase the entry point based on the new load_addr.
	 */
	if (!valid_memory_range(info, load_addr, load_addr + (len + _1MiB))) {
		int ep_offset = ep - load_addr;

		load_addr = locate_hole(info, len + _1MiB, 0, 0, max_addr, 1);
		if (load_addr == ULONG_MAX) {
			printf("Can't allocate memory for kernel of len %ld\n",
					len + _1MiB);
			return -1;
		}

		ep = load_addr + ep_offset;
	}

	add_segment(info, buf, len, load_addr, len + _1MiB);


	if (info->kexec_flags & KEXEC_ON_CRASH) {
                crash_cmdline = xmalloc(COMMAND_LINE_SIZE);
                memset((void *)crash_cmdline, 0, COMMAND_LINE_SIZE);
		ret = load_crashdump_segments(info, crash_cmdline,
						max_addr, 0);
		if (ret < 0) {
			ret = -1;
			goto out;
		}
		crash_cmdline_len = strlen(crash_cmdline);
	} else {
		crash_cmdline = NULL;
		crash_cmdline_len = 0;
	}

	if (crash_cmdline_len + command_line_len + 1 > COMMAND_LINE_SIZE) {
		printf("Kernel command line exceeds maximum possible length\n");
		return -1;
	}

	cmdline_buf = xmalloc(COMMAND_LINE_SIZE);
	memset((void *)cmdline_buf, 0, COMMAND_LINE_SIZE);

	if (command_line)
		strcpy(cmdline_buf, command_line);
	if (crash_cmdline)
		strncat(cmdline_buf, crash_cmdline, crash_cmdline_len);

	elf_rel_build_load(info, &info->rhdr, (const char *)purgatory,
				purgatory_size, 0, -1, -1, 0);

	/* Here we need to initialize the device tree, and find out where
	 * it is going to live so we can place it directly after the
	 * kernel image */
	if (dtb) {
		/* Grab device tree from buffer */
		blob_buf = slurp_file(dtb, &blob_size);
	} else {
		create_flatten_tree(info, (unsigned char **)&blob_buf,
				(unsigned long *)&blob_size, cmdline_buf);
	}
	if (!blob_buf || !blob_size) {
		error_msg = "Device tree seems to be an empty file.\n";
		goto out2;
	}

	/* initial fixup for device tree */
	blob_buf = fixup_dtb_init(info, blob_buf, &blob_size, load_addr, &dtb_addr);

	if (ramdisk) {
		seg_buf = slurp_ramdisk_ppc(ramdisk, &seg_size);
		/* Load ramdisk at top of memory */
		hole_addr = add_buffer(info, seg_buf, seg_size, seg_size,
				0, dtb_addr + blob_size, max_addr, -1);
		ramdisk_base = hole_addr;
		ramdisk_size = seg_size;
	}
	if (reuse_initrd) {
		ramdisk_base = initrd_base;
		ramdisk_size = initrd_size;
	}

	if (info->kexec_flags & KEXEC_ON_CRASH && ramdisk_base != 0) {
		if ( (ramdisk_base < crash_base) ||
		     (ramdisk_base > crash_base + crash_size) ) {
			printf("WARNING: ramdisk is above crashkernel region!\n");
		}
		else if (ramdisk_base + ramdisk_size > crash_base + crash_size) {
			printf("WARNING: ramdisk overflows crashkernel region!\n");
		}
	}

	/* Perform final fixup on devie tree, i.e. everything beside what
	 * was done above */
	fixup_dtb_finalize(info, blob_buf, &blob_size, fixup_nodes,
			cmdline_buf);
	dtb_addr_actual = add_buffer(info, blob_buf, blob_size, blob_size, 0, dtb_addr,
			load_addr + KERNEL_ACCESS_TOP, 1);
	if (dtb_addr_actual != dtb_addr) {
		printf("dtb_addr_actual: %lx, dtb_addr: %lx\n", dtb_addr_actual, dtb_addr);
		error_msg = "Error device tree not loadded to address it was expecting to be loaded too!\n";
		goto out2;
	}

	/* set various variables for the purgatory */
	addr = ep;
	elf_rel_set_symbol(&info->rhdr, "kernel", &addr, sizeof(addr));

	addr = dtb_addr;
	elf_rel_set_symbol(&info->rhdr, "dt_offset", &addr, sizeof(addr));

#define PUL_STACK_SIZE  (16 * 1024)
	addr = locate_hole(info, PUL_STACK_SIZE, 0, 0, -1, 1);
	addr += PUL_STACK_SIZE;
	elf_rel_set_symbol(&info->rhdr, "stack", &addr, sizeof(addr));
	/* No allocation past here in order not to overwrite the stack */
#undef PUL_STACK_SIZE

	/*
	 * Fixup ThreadPointer(r2) for purgatory.
	 * PPC32 ELF ABI expects :
	 * ThreadPointer (TP) = TCB + 0x7000
	 * We manually allocate a TCB space and set the TP
	 * accordingly.
	 */
#define TCB_SIZE 	1024
#define TCB_TP_OFFSET 	0x7000	/* PPC32 ELF ABI */
	addr = locate_hole(info, TCB_SIZE, 0, 0,
				((unsigned long)-1 - TCB_TP_OFFSET),
				1);
	addr += TCB_SIZE + TCB_TP_OFFSET;
	elf_rel_set_symbol(&info->rhdr, "my_thread_ptr", &addr, sizeof(addr));
#undef TCB_TP_OFFSET
#undef TCB_SIZE

	addr = elf_rel_get_addr(&info->rhdr, "purgatory_start");
	info->entry = (void *)addr;

out2:
	free(cmdline_buf);
out:
	free(crash_cmdline);
	if (!tmp_cmdline)
		free(command_line);
	if (error_msg)
		die("%s", error_msg);
	return ret;
}

int uImage_ppc_load(int argc, char **argv, const char *buf, off_t len,
		struct kexec_info *info)
{
	struct Image_info img;
	int ret;

	ret = uImage_load(buf, len, &img);
	if (ret)
		return ret;

	return	ppc_load_bare_bits(argc, argv, img.buf, img.len, info,
				img.base, img.ep);
}
