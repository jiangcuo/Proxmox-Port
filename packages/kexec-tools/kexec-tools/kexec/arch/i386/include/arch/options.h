#ifndef KEXEC_ARCH_I386_OPTIONS_H
#define KEXEC_ARCH_I386_OPTIONS_H

/*
 *************************************************************************
 * NOTE NOTE NOTE
 * This file is included for i386 builds *and* x86_64 builds (which build
 * both x86_64 and i386 loaders).
 * It contains the combined set of options used by i386 and x86_64.
 *************************************************************************
 */

#define OPT_RESET_VGA      (OPT_MAX+0)
#define OPT_SERIAL         (OPT_MAX+1)
#define OPT_SERIAL_BAUD    (OPT_MAX+2)
#define OPT_CONSOLE_VGA    (OPT_MAX+3)
#define OPT_CONSOLE_SERIAL (OPT_MAX+4)
#define OPT_ELF32_CORE     (OPT_MAX+5)
#define OPT_ELF64_CORE     (OPT_MAX+6)
#define OPT_ARCH_MAX       (OPT_MAX+7)

#define OPT_APPEND		(OPT_ARCH_MAX+0)
#define OPT_REUSE_CMDLINE	(OPT_ARCH_MAX+1)
#define OPT_RAMDISK		(OPT_ARCH_MAX+2)
#define OPT_ARGS_ELF    	(OPT_ARCH_MAX+3)
#define OPT_ARGS_LINUX  	(OPT_ARCH_MAX+4)
#define OPT_ARGS_NONE   	(OPT_ARCH_MAX+5)
#define OPT_CL  		(OPT_ARCH_MAX+6)
#define OPT_MOD 		(OPT_ARCH_MAX+7)
#define OPT_VGA 		(OPT_ARCH_MAX+8)
#define OPT_REAL_MODE		(OPT_ARCH_MAX+9)
#define OPT_ENTRY_32BIT		(OPT_ARCH_MAX+10)
#define OPT_PASS_MEMMAP_CMDLINE	(OPT_ARCH_MAX+11)
#define OPT_NOEFI		(OPT_ARCH_MAX+12)
#define OPT_REUSE_VIDEO_TYPE	(OPT_ARCH_MAX+13)
#define OPT_DTB			(OPT_ARCH_MAX+14)

/* Options relevant to the architecture (excluding loader-specific ones): */
#define KEXEC_ARCH_OPTIONS \
	KEXEC_OPTIONS \
	{ "reset-vga",	    0, 0, OPT_RESET_VGA }, \
	{ "serial",	    1, 0, OPT_SERIAL }, \
	{ "serial-baud",    1, 0, OPT_SERIAL_BAUD }, \
	{ "console-vga",    0, 0, OPT_CONSOLE_VGA }, \
	{ "console-serial", 0, 0, OPT_CONSOLE_SERIAL }, \
	{ "elf32-core-headers", 0, 0, OPT_ELF32_CORE }, \
	{ "elf64-core-headers", 0, 0, OPT_ELF64_CORE }, \
	{ "pass-memmap-cmdline", 0, 0, OPT_PASS_MEMMAP_CMDLINE }, \
	{ "noefi", 0, 0, OPT_NOEFI}, \
	{ "reuse-video-type", 0, 0, OPT_REUSE_VIDEO_TYPE },	\

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
#define KEXEC_ALL_OPTIONS \
	KEXEC_ARCH_OPTIONS \
	{ "command-line",	1, NULL, OPT_APPEND },		\
	{ "append",		1, NULL, OPT_APPEND },		\
	{ "reuse-cmdline",	0, NULL, OPT_REUSE_CMDLINE },	\
	{ "initrd",		1, NULL, OPT_RAMDISK },		\
	{ "ramdisk",		1, NULL, OPT_RAMDISK },		\
	{ "args-elf",		0, NULL, OPT_ARGS_ELF },	\
	{ "args-linux",		0, NULL, OPT_ARGS_LINUX },	\
	{ "args-none",		0, NULL, OPT_ARGS_NONE },	\
	{ "module",		1, 0, OPT_MOD },		\
	{ "real-mode",		0, NULL, OPT_REAL_MODE },	\
	{ "entry-32bit",	0, NULL, OPT_ENTRY_32BIT },	\
	{ "dtb",		1, NULL, OPT_DTB },

#define KEXEC_ALL_OPT_STR KEXEC_ARCH_OPT_STR

#endif /* KEXEC_ARCH_I386_OPTIONS_H */

