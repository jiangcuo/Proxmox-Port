#ifndef KEXEC_ARCH_HPPA_OPTIONS_H
#define KEXEC_ARCH_HPPA_OPTIONS_H

#define OPT_ARCH_MAX   (OPT_MAX+0)

#define KEXEC_ARCH_OPTIONS \
	KEXEC_OPTIONS

#define KEXEC_ARCH_OPT_STR KEXEC_OPT_STR ""

#define KEXEC_ALL_OPTIONS \
	KEXEC_ARCH_OPTIONS \
	{ "command-line",	1, 0, OPT_APPEND },		\
	{ "reuse-cmdline",	0, 0, OPT_REUSE_CMDLINE },	\
	{ "append",		1, 0, OPT_APPEND },		\
	{ "initrd",		1, 0, OPT_RAMDISK },		\
	{ "ramdisk",		1, 0, OPT_RAMDISK },


#define KEXEC_ALL_OPT_STR KEXEC_ARCH_OPT_STR ""

/* See the other architectures for details of these; HPPA has no
 * loader-specific options yet.
 */
#define OPT_ARCH_MAX       (OPT_MAX+0)

#define OPT_APPEND		(OPT_ARCH_MAX+0)
#define OPT_REUSE_CMDLINE	(OPT_ARCH_MAX+1)
#define OPT_RAMDISK		(OPT_ARCH_MAX+2)

#define MAX_MEMORY_RANGES 16
#endif /* KEXEC_ARCH_HPPA_OPTIONS_H */
