#
# kexec_test Debugging payload to be certain the infrastructure works
#
RELOC:=0x10000
KEXEC_TEST_SRCS:= kexec_test/kexec_test16.S kexec_test/kexec_test.S

dist += kexec_test/Makefile $(KEXEC_TEST_SRCS)				\
	kexec_test/x86-setup-legacy-pic.S

BUILD_KEXEC_TEST = no
ifeq ($(ARCH),i386)
BUILD_KEXEC_TEST = yes
endif
ifeq ($(ARCH),x86_64)
BUILD_KEXEC_TEST = yes
endif

ifeq ($(BUILD_KEXEC_TEST),yes)

KEXEC_TEST_OBJS = $(call objify, $(KEXEC_TEST_SRCS))
KEXEC_TEST_DEPS = $(call depify, $(KEXEC_TEST_OBJS))

KEXEC_TEST = $(PKGLIBDIR)/kexec_test

clean += $(KEXEC_TEST_OBJS) $(KEXEC_TEST_DEPS) $(KEXEC_TEST)

-include $(KEXEC_TEST_DEPS)

$(KEXEC_TEST): CC=$(TARGET_CC)
$(KEXEC_TEST): CPPFLAGS+=-DRELOC=$(RELOC)
$(KEXEC_TEST): ASFLAGS+=-m32
#$(KEXEC_TEST): LDFLAGS=-m32 -Wl,-e -Wl,_start -Wl,-Ttext -Wl,$(RELOC) \
#		       -nostartfiles
$(KEXEC_TEST): LDFLAGS=-melf_i386 -e _start -Ttext $(RELOC)

$(KEXEC_TEST): $(KEXEC_TEST_OBJS)
	mkdir -p $(@D)
	$(TARGET_LD) $(LDFLAGS) -o $@ $^

endif
