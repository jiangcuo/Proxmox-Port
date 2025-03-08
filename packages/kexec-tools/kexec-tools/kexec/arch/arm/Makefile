#
# kexec arm (linux booting linux)
#
include $(srcdir)/kexec/libfdt/Makefile.libfdt

arm_FS2DT              = kexec/fs2dt.c
arm_FS2DT_INCLUDE      = -include $(srcdir)/kexec/arch/arm/crashdump-arm.h \
                         -include $(srcdir)/kexec/arch/arm/kexec-arm.h

arm_MEM_REGIONS        = kexec/mem_regions.c

arm_KEXEC_SRCS=  kexec/arch/arm/kexec-elf-rel-arm.c
arm_KEXEC_SRCS+= kexec/arch/arm/kexec-zImage-arm.c
arm_KEXEC_SRCS+= kexec/arch/arm/kexec-uImage-arm.c
arm_KEXEC_SRCS+= kexec/arch/arm/kexec-arm.c
arm_KEXEC_SRCS+= kexec/arch/arm/crashdump-arm.c
arm_KEXEC_SRCS+= kexec/fs2dt.c

libfdt_SRCS += $(LIBFDT_SRCS:%=kexec/libfdt/%)

arm_CPPFLAGS = -I$(srcdir)/kexec/libfdt

# We want 64-bit file IO for kdump to work correctly on LPAE systems
arm_CPPFLAGS += -D_FILE_OFFSET_BITS=64

arm_KEXEC_SRCS += $(libfdt_SRCS)

arm_UIMAGE = kexec/kexec-uImage.c
arm_PHYS_TO_VIRT = kexec/arch/arm/phys_to_virt.c

dist += kexec/arch/arm/Makefile $(arm_KEXEC_SRCS) $(arm_PHYS_TO_VIRT)	\
	kexec/arch/arm/iomem.h kexec/arch/arm/phys_to_virt.h \
	kexec/arch/arm/crashdump-arm.h kexec/arch/arm/kexec-arm.h \
	kexec/arch/arm/include/arch/options.h
