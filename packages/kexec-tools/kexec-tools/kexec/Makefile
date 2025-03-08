#
# kexec (linux booting linux)
#
PURGATORY_HEX_C = kexec/purgatory.c

$(PURGATORY_HEX_C): $(PURGATORY) $(BIN_TO_HEX)
	$(MKDIR) -p $(@D)
	$(BIN_TO_HEX) purgatory < $(PURGATORY) > $@

KEXEC_SRCS = $(KEXEC_SRCS_base)
KEXEC_GENERATED_SRCS =

KEXEC_SRCS_base += kexec/kexec.c
KEXEC_SRCS_base += kexec/ifdown.c
KEXEC_SRCS_base += kexec/kexec-elf.c
KEXEC_SRCS_base += kexec/kexec-elf-exec.c
KEXEC_SRCS_base += kexec/kexec-elf-core.c
KEXEC_SRCS_base += kexec/kexec-elf-rel.c
KEXEC_SRCS_base += kexec/kexec-elf-boot.c
KEXEC_SRCS_base += kexec/kexec-pe-zboot.c
KEXEC_SRCS_base += kexec/kexec-iomem.c
KEXEC_SRCS_base += kexec/firmware_memmap.c
KEXEC_SRCS_base += kexec/crashdump.c
KEXEC_SRCS_base += kexec/crashdump-xen.c
KEXEC_SRCS_base += kexec/phys_arch.c
KEXEC_SRCS_base += kexec/lzma.c
KEXEC_SRCS_base += kexec/zlib.c
KEXEC_SRCS_base += kexec/kexec-xen.c
KEXEC_SRCS_base += kexec/symbols.c

KEXEC_GENERATED_SRCS += $(PURGATORY_HEX_C)

dist += kexec/Makefile						\
	$(KEXEC_SRCS_base) kexec/crashdump-elf.c		\
	kexec/crashdump.h kexec/firmware_memmap.h		\
	kexec/kexec-elf-boot.h					\
	kexec/kexec-elf.h kexec/kexec-sha256.h			\
	kexec/kexec-zlib.h kexec/kexec-lzma.h			\
	kexec/kexec-xen.h 					\
	kexec/kexec-syscall.h kexec/kexec.h kexec/kexec.8

dist				+= kexec/proc_iomem.c
$(ARCH)_PROC_IOMEM		=  kexec/proc_iomem.c
KEXEC_SRCS			+= $($(ARCH)_PROC_IOMEM)

dist				+= kexec/virt_to_phys.c
$(ARCH)_VIRT_TO_PHYS		=  kexec/virt_to_phys.c
KEXEC_SRCS			+= $($(ARCH)_VIRT_TO_PHYS)

dist				+= kexec/phys_to_virt.c
$(ARCH)_PHYS_TO_VIRT		=  kexec/phys_to_virt.c
KEXEC_SRCS			+= $($(ARCH)_PHYS_TO_VIRT)

dist				+= kexec/add_segment.c
$(ARCH)_ADD_SEGMENT		=  kexec/add_segment.c
KEXEC_SRCS			+= $($(ARCH)_ADD_SEGMENT)

dist				+= kexec/add_buffer.c
$(ARCH)_ADD_BUFFER		=  kexec/add_buffer.c
KEXEC_SRCS			+= $($(ARCH)_ADD_BUFFER)

dist				+=  kexec/arch_reuse_initrd.c
$(ARCH)_ARCH_REUSE_INITRD	=  kexec/arch_reuse_initrd.c
KEXEC_SRCS			+= $($(ARCH)_ARCH_REUSE_INITRD)

dist				+= kexec/kexec-uImage.c
$(ARCH)_UIMAGE			=
KEXEC_SRCS			+= $($(ARCH)_UIMAGE)

dist				+= kexec/fs2dt.c kexec/fs2dt.h
$(ARCH)_FS2DT			=
KEXEC_SRCS			+= $($(ARCH)_FS2DT)

dist				+= kexec/mem_regions.c kexec/mem_regions.h
$(ARCH)_MEM_REGIONS		=
KEXEC_SRCS			+= $($(ARCH)_MEM_REGIONS)

dist				+= kexec/dt-ops.c kexec/dt-ops.h
$(ARCH)_DT_OPS		=
KEXEC_SRCS			+= $($(ARCH)_DT_OPS)

include $(srcdir)/kexec/arch/alpha/Makefile
include $(srcdir)/kexec/arch/arm/Makefile
include $(srcdir)/kexec/arch/arm64/Makefile
include $(srcdir)/kexec/arch/i386/Makefile
include $(srcdir)/kexec/arch/ia64/Makefile
include $(srcdir)/kexec/arch/m68k/Makefile
include $(srcdir)/kexec/arch/mips/Makefile
include $(srcdir)/kexec/arch/cris/Makefile
include $(srcdir)/kexec/arch/ppc/Makefile
include $(srcdir)/kexec/arch/ppc64/Makefile
include $(srcdir)/kexec/arch/s390/Makefile
include $(srcdir)/kexec/arch/sh/Makefile
include $(srcdir)/kexec/arch/x86_64/Makefile
include $(srcdir)/kexec/arch/hppa/Makefile
include $(srcdir)/kexec/arch/loongarch/Makefile

KEXEC_SRCS += $($(ARCH)_KEXEC_SRCS)

KEXEC_OBJS = $(call objify, $(KEXEC_SRCS) $(KEXEC_GENERATED_SRCS))
KEXEC_DEPS = $(call depify, $(KEXEC_OBJS))

clean += $(KEXEC_OBJS) $(KEXEC_DEPS) $(KEXEC_GENERATED_SRCS) \
	 $(KEXEC) $(KEXEC_MANPAGE)

KEXEC = $(SBINDIR)/kexec
KEXEC_MANPAGE = $(MANDIR)/man8/kexec.8

-include $(KEXEC_DEPS)

$(KEXEC): $(KEXEC_OBJS) $(UTIL_LIB)
	@$(MKDIR) -p $(@D)
	$(LINK.o) -o $@ $^ $(CFLAGS) $(LIBS)

$(KEXEC): CPPFLAGS+=-I$(srcdir)/kexec/arch/$(ARCH)/include

kexec/fs2dt.o: CPPFLAGS+=$($(ARCH)_FS2DT_INCLUDE)

$(KEXEC_MANPAGE): kexec/kexec.8
	@$(MKDIR) -p     $(MANDIR)/man8
	cp $^ $(KEXEC_MANPAGE)
echo::
	@echo "KEXEC_SRCS $(KEXEC_SRCS)"
	@echo "KEXEC_DEPS $(KEXEC_DEPS)"
	@echo "KEXEC_OBJS $(KEXEC_OBJS)"

