#
# kexec sh (linux booting linux)
#
sh_KEXEC_SRCS += kexec/arch/sh/kexec-sh.c
sh_KEXEC_SRCS += kexec/arch/sh/kexec-uImage-sh.c
sh_KEXEC_SRCS += kexec/arch/sh/kexec-zImage-sh.c
sh_KEXEC_SRCS += kexec/arch/sh/kexec-netbsd-sh.c
sh_KEXEC_SRCS += kexec/arch/sh/kexec-elf-sh.c
sh_KEXEC_SRCS += kexec/arch/sh/kexec-elf-rel-sh.c
sh_KEXEC_SRCS += kexec/arch/sh/netbsd_booter.S
sh_KEXEC_SRCS += kexec/arch/sh/crashdump-sh.c

sh_UIMAGE = kexec/kexec-uImage.c

sh_ADD_BUFFER =
sh_ADD_SEGMENT =
sh_VIRT_TO_PHYS =

dist += kexec/arch/sh/Makefile $(sh_KEXEC_SRCS)				\
	kexec/arch/sh/kexec-sh.h					\
	kexec/arch/sh/crashdump-sh.h					\
	kexec/arch/sh/include/arch/options.h
