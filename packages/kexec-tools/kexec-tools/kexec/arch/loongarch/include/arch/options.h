#ifndef KEXEC_ARCH_LOONGARCH_OPTIONS_H
#define KEXEC_ARCH_LOONGARCH_OPTIONS_H

#define OPT_APPEND		((OPT_MAX)+0)
#define OPT_INITRD		((OPT_MAX)+1)
#define OPT_REUSE_CMDLINE	((OPT_MAX)+2)
#define OPT_ARCH_MAX		((OPT_MAX)+3)

#define KEXEC_ARCH_OPTIONS \
	KEXEC_OPTIONS \
	{ "append",        1, NULL, OPT_APPEND }, \
	{ "command-line",  1, NULL, OPT_APPEND }, \
	{ "initrd",        1, NULL, OPT_INITRD }, \
	{ "ramdisk",       1, NULL, OPT_INITRD }, \
	{ "reuse-cmdline", 0, NULL, OPT_REUSE_CMDLINE }, \

#define KEXEC_ARCH_OPT_STR KEXEC_OPT_STR /* Only accept long arch options. */
#define KEXEC_ALL_OPTIONS KEXEC_ARCH_OPTIONS
#define KEXEC_ALL_OPT_STR KEXEC_ARCH_OPT_STR

static const char loongarch_opts_usage[] __attribute__ ((unused)) =
"     --append=STRING       Set the kernel command line to STRING.\n"
"     --command-line=STRING Set the kernel command line to STRING.\n"
"     --initrd=FILE         Use FILE as the kernel initial ramdisk.\n"
"     --ramdisk=FILE        Use FILE as the kernel initial ramdisk.\n"
"     --reuse-cmdline       Use kernel command line from running system.\n";

#endif /* KEXEC_ARCH_LOONGARCH_OPTIONS_H */
