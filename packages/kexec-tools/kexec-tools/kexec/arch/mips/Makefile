#
# kexec mips (linux booting linux)
#
mips_KEXEC_SRCS =  kexec/arch/mips/kexec-mips.c
mips_KEXEC_SRCS += kexec/arch/mips/kexec-elf-mips.c
mips_KEXEC_SRCS += kexec/arch/mips/kexec-elf-rel-mips.c
mips_KEXEC_SRCS += kexec/arch/mips/crashdump-mips.c

mips_FS2DT = kexec/fs2dt.c
mips_FS2DT_INCLUDE = \
	-include $(srcdir)/kexec/arch/mips/crashdump-mips.h \
	-include $(srcdir)/kexec/arch/mips/kexec-mips.h

mips_DT_OPS += kexec/dt-ops.c

include $(srcdir)/kexec/libfdt/Makefile.libfdt

libfdt_SRCS += $(LIBFDT_SRCS:%=kexec/libfdt/%)
mips_CPPFLAGS += -I$(srcdir)/kexec/libfdt
mips_KEXEC_SRCS += $(libfdt_SRCS)

mips_ADD_BUFFER =
mips_ADD_SEGMENT =
mips_VIRT_TO_PHYS =

dist += kexec/arch/mips/Makefile $(mips_KEXEC_SRCS)			\
	kexec/arch/mips/kexec-mips.h					\
	kexec/arch/mips/crashdump-mips.h				\
	kexec/arch/mips/include/arch/options.h
