#
# kexec i386 (linux booting linux)
#
i386_KEXEC_SRCS =  kexec/arch/i386/kexec-x86.c
i386_KEXEC_SRCS += kexec/arch/i386/kexec-x86-common.c
i386_KEXEC_SRCS += kexec/arch/i386/kexec-elf-x86.c
i386_KEXEC_SRCS += kexec/arch/i386/kexec-elf-rel-x86.c
i386_KEXEC_SRCS += kexec/arch/i386/kexec-bzImage.c
i386_KEXEC_SRCS += kexec/arch/i386/kexec-multiboot-x86.c
i386_KEXEC_SRCS += kexec/arch/i386/kexec-mb2-x86.c
i386_KEXEC_SRCS += kexec/arch/i386/kexec-beoboot-x86.c
i386_KEXEC_SRCS += kexec/arch/i386/kexec-nbi.c
i386_KEXEC_SRCS += kexec/arch/i386/x86-linux-setup.c
i386_KEXEC_SRCS += kexec/arch/i386/crashdump-x86.c

dist += kexec/arch/i386/Makefile $(i386_KEXEC_SRCS)			\
	kexec/arch/i386/crashdump-x86.h					\
	kexec/arch/i386/kexec-x86.h					\
	kexec/arch/i386/x86-linux-setup.h				\
	kexec/arch/i386/include/arch/options.h
