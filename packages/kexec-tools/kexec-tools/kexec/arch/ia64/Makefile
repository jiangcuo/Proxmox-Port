#
# kexec ia64 (linux booting linux)
#
ia64_KEXEC_SRCS =  kexec/arch/ia64/kexec-iomem.c
ia64_KEXEC_SRCS += kexec/arch/ia64/kexec-ia64.c
ia64_KEXEC_SRCS += kexec/arch/ia64/kexec-elf-ia64.c
ia64_KEXEC_SRCS += kexec/arch/ia64/kexec-elf-rel-ia64.c
ia64_KEXEC_SRCS += kexec/arch/ia64/crashdump-ia64.c

ia64_PROC_IOMEM =

dist += kexec/arch/ia64/Makefile $(ia64_KEXEC_SRCS)			\
	kexec/arch/ia64/kexec-ia64.h kexec/arch/ia64/crashdump-ia64.h	\
	kexec/arch/ia64/include/arch/options.h


