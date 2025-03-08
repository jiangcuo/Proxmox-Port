#ifndef KEXEC_ARCH_ALPHA_OPTIONS_H
#define KEXEC_ARCH_ALPHA_OPTIONS_H

#define OPT_ARCH_MAX   (OPT_MAX+0)

/* Options relevant to the architecture (excluding loader-specific ones),
 * in this case none:
 */
#define KEXEC_ARCH_OPTIONS \
	KEXEC_OPTIONS \

#define KEXEC_ARCH_OPT_STR KEXEC_OPT_STR ""

/* See the other architectures for details of these; Alpha has no
 * loader-specific options yet.
 */
#define KEXEC_ALL_OPTIONS KEXEC_ARCH_OPTIONS
#define KEXEC_ALL_OPT_STR KEXEC_ARCH_OPT_STR

#endif /* KEXEC_ARCH_ALPHA_OPTIONS_H */
