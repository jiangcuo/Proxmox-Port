
arm64_FS2DT += kexec/fs2dt.c
arm64_FS2DT_INCLUDE += \
	-include $(srcdir)/kexec/arch/arm64/crashdump-arm64.h \
	-include $(srcdir)/kexec/arch/arm64/kexec-arm64.h

arm64_DT_OPS += kexec/dt-ops.c

arm64_MEM_REGIONS = kexec/mem_regions.c

arm64_CPPFLAGS += -I $(srcdir)/kexec/

arm64_KEXEC_SRCS += \
	kexec/arch/arm64/crashdump-arm64.c \
	kexec/arch/arm64/kexec-arm64.c \
	kexec/arch/arm64/kexec-elf-arm64.c \
	kexec/arch/arm64/kexec-uImage-arm64.c \
	kexec/arch/arm64/kexec-image-arm64.c \
	kexec/arch/arm64/kexec-vmlinuz-arm64.c

arm64_UIMAGE = kexec/kexec-uImage.c

arm64_ARCH_REUSE_INITRD =
arm64_ADD_SEGMENT =
arm64_VIRT_TO_PHYS =
arm64_PHYS_TO_VIRT =

dist += $(arm64_KEXEC_SRCS) \
	kexec/arch/arm64/include/arch/options.h \
	kexec/arch/arm64/crashdump-arm64.h \
	kexec/arch/arm64/image-header.h \
	kexec/arch/arm64/iomem.h \
	kexec/arch/arm64/kexec-arm64.h \
	kexec/arch/arm64/Makefile

ifdef HAVE_LIBFDT

LIBS += -lfdt

else

include $(srcdir)/kexec/libfdt/Makefile.libfdt

libfdt_SRCS += $(LIBFDT_SRCS:%=kexec/libfdt/%)

arm64_CPPFLAGS += -I$(srcdir)/kexec/libfdt

arm64_KEXEC_SRCS += $(libfdt_SRCS)

endif
