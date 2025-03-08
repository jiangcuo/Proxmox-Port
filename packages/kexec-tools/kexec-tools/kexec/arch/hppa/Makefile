#
# kexec hppa (linux booting linux)
#
hppa_KEXEC_SRCS =  kexec/arch/hppa/kexec-hppa.c
hppa_KEXEC_SRCS += kexec/arch/hppa/kexec-elf-hppa.c
hppa_KEXEC_SRCS += kexec/arch/hppa/kexec-elf-rel-hppa.c

hppa_ADD_SEGMENT =
hppa_VIRT_TO_PHYS =

dist += kexec/arch/hppa/Makefile $(hppa_KEXEC_SRCS)			\
	kexec/arch/hppa/kexec-hppa.h					\
	kexec/arch/hppa/include/arch/options.h
