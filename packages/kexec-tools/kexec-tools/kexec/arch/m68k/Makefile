#
# kexec m68k (linux booting linux)
#
m68k_KEXEC_SRCS =  kexec/arch/m68k/kexec-m68k.c
m68k_KEXEC_SRCS += kexec/arch/m68k/kexec-elf-m68k.c
m68k_KEXEC_SRCS += kexec/arch/m68k/kexec-elf-rel-m68k.c
m68k_KEXEC_SRCS += kexec/arch/m68k/bootinfo.c

m68k_ADD_SEGMENT =
m68k_VIRT_TO_PHYS =

dist += kexec/arch/m68k/Makefile $(m68k_KEXEC_SRCS)			\
	kexec/arch/m68k/bootinfo.h					\
	kexec/arch/m68k/kexec-m68k.h					\
	kexec/arch/m68k/include/arch/options.h
