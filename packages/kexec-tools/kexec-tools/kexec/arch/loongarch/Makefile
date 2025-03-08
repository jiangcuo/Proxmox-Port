#
# kexec loongarch (linux booting linux)
#
loongarch_KEXEC_SRCS =  kexec/arch/loongarch/kexec-loongarch.c
loongarch_KEXEC_SRCS += kexec/arch/loongarch/kexec-elf-loongarch.c
loongarch_KEXEC_SRCS += kexec/arch/loongarch/kexec-pei-loongarch.c
loongarch_KEXEC_SRCS += kexec/arch/loongarch/kexec-elf-rel-loongarch.c
loongarch_KEXEC_SRCS += kexec/arch/loongarch/crashdump-loongarch.c
loongarch_KEXEC_SRCS += kexec/arch/loongarch/kexec-pez-loongarch.c

loongarch_MEM_REGIONS = kexec/mem_regions.c

loongarch_CPPFLAGS += -I $(srcdir)/kexec/

loongarch_ADD_BUFFER =
loongarch_ADD_SEGMENT =
loongarch_VIRT_TO_PHYS =

dist += kexec/arch/loongarch/Makefile $(loongarch_KEXEC_SRCS)			\
	kexec/arch/loongarch/kexec-loongarch.h					\
	kexec/arch/loongarch/image-header.h					\
	kexec/arch/loongarch/iomem.h						\
	kexec/arch/loongarch/crashdump-loongarch.h				\
	kexec/arch/loongarch/include/arch/options.h
