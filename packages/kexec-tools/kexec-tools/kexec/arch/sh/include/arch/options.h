#ifndef KEXEC_ARCH_SH_OPTIONS_H
#define KEXEC_ARCH_SH_OPTIONS_H

#define OPT_ARCH_MAX     (OPT_MAX+0)
#define OPT_APPEND       (OPT_ARCH_MAX+1)
#define OPT_EMPTYZERO    (OPT_ARCH_MAX+2)
#define OPT_NBSD_HOWTO   (OPT_ARCH_MAX+3)
#define OPT_NBSD_MROOT   (OPT_ARCH_MAX+4)

/* Options relevant to the architecture (excluding loader-specific ones): */
#define KEXEC_ARCH_OPTIONS \
	KEXEC_OPTIONS \
        {"command-line",   1, 0, OPT_APPEND}, \
        {"append",         1, 0, OPT_APPEND}, \
        {"empty-zero",     1, 0, OPT_APPEND}, \
        {"howto",          1, 0, OPT_NBSD_HOWTO}, \
        {"miniroot",       1, 0, OPT_NBSD_MROOT},
/* These options seem to be loader-specific rather than superh-specific, so
 * ought to be moved to KEXEC_ALL_OPTIONS below and parsed in the relevant
 * loader, e.g. kexec-netbsd-sh.c
 */

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
#define KEXEC_ALL_OPTIONS KEXEC_ARCH_OPTIONS
#define KEXEC_ALL_OPT_STR KEXEC_ARCH_OPT_STR

#endif /* KEXEC_ARCH_SH_OPTIONS_H */
