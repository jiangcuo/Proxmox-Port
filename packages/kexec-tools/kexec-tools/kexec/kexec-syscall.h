#ifndef KEXEC_SYSCALL_H
#define KEXEC_SYSCALL_H

#define __LIBRARY__
#include <sys/syscall.h>
#include <unistd.h>

#define LINUX_REBOOT_CMD_KEXEC_OLD	0x81726354
#define LINUX_REBOOT_CMD_KEXEC_OLD2	0x18263645
#define LINUX_REBOOT_CMD_KEXEC		0x45584543

#ifndef __NR_kexec_load
#ifdef __i386__
#define __NR_kexec_load		283
#endif
#ifdef __sh__
#define __NR_kexec_load		283
#endif
#ifdef __cris__
#ifndef __NR_kexec_load
#define __NR_kexec_load		283
#endif
#endif
#ifdef __ia64__
#define __NR_kexec_load		1268
#endif
#ifdef __powerpc64__
#define __NR_kexec_load		268
#endif
#ifdef __powerpc__
#define __NR_kexec_load		268
#endif
#ifdef __x86_64__
#define __NR_kexec_load		246
#endif
#ifdef __s390x__
#define __NR_kexec_load		277
#endif
#ifdef __s390__
#define __NR_kexec_load		277
#endif
#ifdef __loongarch__
#define __NR_kexec_load		104
#endif
#if defined(__arm__) || defined(__arm64__)
#define __NR_kexec_load		__NR_SYSCALL_BASE + 347
#endif
#if defined(__mips__)
#define __NR_kexec_load                4311
#endif
#ifdef __m68k__
#define __NR_kexec_load                313
#endif
#ifdef __alpha__
#define __NR_kexec_load                448
#endif
#ifndef __NR_kexec_load
#error Unknown processor architecture.  Needs a kexec_load syscall number.
#endif
#endif /*ifndef __NR_kexec_load*/

#if defined(__arm__) || defined(__loongarch__)
#undef __NR_kexec_file_load
#endif

#ifndef __NR_kexec_file_load

#ifdef __x86_64__
#define __NR_kexec_file_load	320
#endif
#ifdef __powerpc64__
#define __NR_kexec_file_load	382
#endif
#ifdef __s390x__
#define __NR_kexec_file_load	381
#endif
#ifdef __aarch64__
#define __NR_kexec_file_load	294
#endif
#ifdef __hppa__
#define __NR_kexec_file_load	355
#endif

#ifndef __NR_kexec_file_load
/* system call not available for the arch */
#define __NR_kexec_file_load	0xffffffff	/* system call not available */
#endif

#endif /*ifndef __NR_kexec_file_load*/

struct kexec_segment;

static inline long kexec_load(void *entry, unsigned long nr_segments,
			struct kexec_segment *segments, unsigned long flags)
{
	return (long) syscall(__NR_kexec_load, entry, nr_segments, segments, flags);
}

static inline int is_kexec_file_load_implemented(void) {
	if (__NR_kexec_file_load != 0xffffffff)
		return 1;
	return 0;
}

static inline long kexec_file_load(int kernel_fd, int initrd_fd,
			unsigned long cmdline_len, const char *cmdline_ptr,
			unsigned long flags)
{
	return (long) syscall(__NR_kexec_file_load, kernel_fd, initrd_fd,
				cmdline_len, cmdline_ptr, flags);
}

#define KEXEC_ON_CRASH		0x00000001
#define KEXEC_PRESERVE_CONTEXT	0x00000002
#define KEXEC_UPDATE_ELFCOREHDR	0x00000004
#define KEXEC_CRASH_HOTPLUG_SUPPORT	0x00000008
#define KEXEC_ARCH_MASK		0xffff0000

/* Flags for kexec file based system call */
#define KEXEC_FILE_UNLOAD	0x00000001
#define KEXEC_FILE_ON_CRASH	0x00000002
#define KEXEC_FILE_NO_INITRAMFS	0x00000004
#define KEXEC_FILE_DEBUG	0x00000008

/* These values match the ELF architecture values. 
 * Unless there is a good reason that should continue to be the case.
 */
#define KEXEC_ARCH_DEFAULT ( 0 << 16)
#define KEXEC_ARCH_386     ( 3 << 16)
#define KEXEC_ARCH_68K     ( 4 << 16)
#define KEXEC_ARCH_HPPA    (15 << 16)
#define KEXEC_ARCH_X86_64  (62 << 16)
#define KEXEC_ARCH_PPC     (20 << 16)
#define KEXEC_ARCH_PPC64   (21 << 16)
#define KEXEC_ARCH_IA_64   (50 << 16)
#define KEXEC_ARCH_ARM     (40 << 16)
#define KEXEC_ARCH_ARM64   (183 << 16)
#define KEXEC_ARCH_S390    (22 << 16)
#define KEXEC_ARCH_SH      (42 << 16)
#define KEXEC_ARCH_MIPS_LE (10 << 16)
#define KEXEC_ARCH_MIPS    ( 8 << 16)
#define KEXEC_ARCH_CRIS    (76 << 16)
#define KEXEC_ARCH_LOONGARCH	(258 << 16)

#define KEXEC_MAX_SEGMENTS 16

#ifdef __i386__
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_386
#endif
#ifdef __sh__
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_SH
#endif
#ifdef __cris__
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_CRIS
#endif
#ifdef __ia64__
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_IA_64
#endif
#ifdef __powerpc64__
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_PPC64
#else
 #ifdef __powerpc__
 #define KEXEC_ARCH_NATIVE	KEXEC_ARCH_PPC
 #endif
#endif
#ifdef __x86_64__
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_X86_64
#endif
#ifdef __s390x__
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_S390
#endif
#ifdef __s390__
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_S390
#endif
#ifdef __arm__
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_ARM
#endif
#if defined(__mips__)
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_MIPS
#endif
#ifdef __m68k__
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_68K
#endif
#if defined(__arm64__)
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_ARM64
#endif
#if defined(__loongarch__)
#define KEXEC_ARCH_NATIVE	KEXEC_ARCH_LOONGARCH
#endif

#endif /* KEXEC_SYSCALL_H */
