#if !defined(KEXEC_ARCH_ARM64_OPTIONS_H)
#define KEXEC_ARCH_ARM64_OPTIONS_H

#define OPT_APPEND		((OPT_MAX)+0)
#define OPT_DTB			((OPT_MAX)+1)
#define OPT_INITRD		((OPT_MAX)+2)
#define OPT_REUSE_CMDLINE	((OPT_MAX)+3)
#define OPT_SERIAL		((OPT_MAX)+4)
#define OPT_ARCH_MAX		((OPT_MAX)+5)

#define KEXEC_ARCH_OPTIONS \
	KEXEC_OPTIONS \
	{ "append",        1, NULL, OPT_APPEND }, \
	{ "command-line",  1, NULL, OPT_APPEND }, \
	{ "dtb",           1, NULL, OPT_DTB }, \
	{ "initrd",        1, NULL, OPT_INITRD }, \
	{ "serial",        1, NULL, OPT_SERIAL }, \
	{ "ramdisk",       1, NULL, OPT_INITRD }, \
	{ "reuse-cmdline", 0, NULL, OPT_REUSE_CMDLINE }, \

#define KEXEC_ARCH_OPT_STR KEXEC_OPT_STR /* Only accept long arch options. */
#define KEXEC_ALL_OPTIONS KEXEC_ARCH_OPTIONS
#define KEXEC_ALL_OPT_STR KEXEC_ARCH_OPT_STR

static const char arm64_opts_usage[] __attribute__ ((unused)) =
"     --append=STRING       Set the kernel command line to STRING.\n"
"     --command-line=STRING Set the kernel command line to STRING.\n"
"     --dtb=FILE            Use FILE as the device tree blob.\n"
"     --initrd=FILE         Use FILE as the kernel initial ramdisk.\n"
"     --serial=STRING       Name of console used for purgatory printing. (e.g. ttyAMA0)\n"
"     --ramdisk=FILE        Use FILE as the kernel initial ramdisk.\n"
"     --reuse-cmdline       Use kernel command line from running system.\n";

struct arm64_opts {
	const char *command_line;
	const char *dtb;
	const char *initrd;
	const char *console;
};

extern struct arm64_opts arm64_opts;

#endif
