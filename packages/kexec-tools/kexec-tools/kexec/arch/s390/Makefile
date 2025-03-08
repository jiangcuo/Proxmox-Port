#
# kexec s390 (linux booting linux)
#
s390_KEXEC_SRCS =  kexec/arch/s390/kexec-s390.c
s390_KEXEC_SRCS += kexec/arch/s390/kexec-image.c
s390_KEXEC_SRCS += kexec/arch/s390/kexec-elf-rel-s390.c
s390_KEXEC_SRCS += kexec/arch/s390/crashdump-s390.c

dist += kexec/arch/s390/Makefile $(s390_KEXEC_SRCS)			\
	kexec/arch/s390/kexec-s390.h					\
	kexec/arch/s390/include/arch/options.h
