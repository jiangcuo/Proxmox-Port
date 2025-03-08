#ifndef KEXEC_ARCH_ARM_OPTIONS_H
#define KEXEC_ARCH_ARM_OPTIONS_H

#define OPT_DT_NO_OLD_ROOT	(OPT_MAX+0)
#define OPT_ARCH_MAX		(OPT_MAX+1)

#define OPT_DTB		(OPT_ARCH_MAX+0)
#define OPT_ATAGS	(OPT_ARCH_MAX+1)
#define OPT_IMAGE_SIZE	(OPT_ARCH_MAX+2)
#define OPT_PAGE_OFFSET	(OPT_ARCH_MAX+3)
#define OPT_APPEND	(OPT_ARCH_MAX+4)
#define OPT_RAMDISK	(OPT_ARCH_MAX+5)

/* Options relevant to the architecture (excluding loader-specific ones),
 * in this case none:
 */
#define KEXEC_ARCH_OPTIONS \
	KEXEC_OPTIONS \
	{ "dt-no-old-root",	0, 0, OPT_DT_NO_OLD_ROOT }, \

#define KEXEC_ARCH_OPT_STR KEXEC_OPT_STR ""

/* The following two #defines list ALL of the options added by all of the
 * architecture's loaders.
 * o	main() uses this complete list to scan for its options, ignoring
 *	arch-specific/loader-specific ones.
 * o	Then, arch_process_options() uses this complete list to scan for its
 *	options, ignoring general/loader-specific ones.
 * o	Then, the file_type[n].load re-scans for options, using
 *	KEXEC_ARCH_OPTIONS plus its loader-specific options subset.
 *	Any unrecognised options cause an error here.
 *
 * This is done so that main()'s/arch_process_options()'s getopt_long() calls
 * don't choose a kernel filename from random arguments to options they don't
 * recognise -- as they now recognise (if not act upon) all possible options.
 */
#define KEXEC_ALL_OPTIONS				\
	KEXEC_ARCH_OPTIONS				\
	{ "command-line",	1, 0, OPT_APPEND },	\
	{ "append",		1, 0, OPT_APPEND },	\
	{ "initrd",		1, 0, OPT_RAMDISK },	\
	{ "ramdisk",		1, 0, OPT_RAMDISK },	\
	{ "dtb",		1, 0, OPT_DTB }, 	\
	{ "atags",		0, 0, OPT_ATAGS },	\
	{ "image-size",		1, 0, OPT_IMAGE_SIZE }, \
	{ "page-offset",	1, 0, OPT_PAGE_OFFSET },

#define KEXEC_ALL_OPT_STR KEXEC_ARCH_OPT_STR ""

extern unsigned int kexec_arm_image_size;

#endif /* KEXEC_ARCH_ARM_OPTIONS_H */
