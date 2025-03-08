#
# Purgatory ppc64
#

ppc64_PURGATORY_SRCS += purgatory/arch/ppc64/v2wrap.S
ppc64_PURGATORY_SRCS += purgatory/arch/ppc64/hvCall.S
ppc64_PURGATORY_SRCS += purgatory/arch/ppc64/purgatory-ppc64.c
ppc64_PURGATORY_SRCS += purgatory/arch/ppc64/console-ppc64.c
ppc64_PURGATORY_SRCS += purgatory/arch/ppc64/crashdump_backup.c
ppc64_PURGATORY_SRCS += purgatory/arch/ppc64/misc.S

ppc64_PURGATORY_EXTRA_CFLAGS += -m64 -msoft-float \
				-fno-exceptions
ppc64_PURGATORY_EXTRA_ASFLAGS += -m64
ifeq ($(SUBARCH),BE)
	ppc64_PURGATORY_EXTRA_LDFLAGS += -melf64ppc
	ppc64_PURGATORY_EXTRA_CFLAGS += -mcall-aixdesc
	ppc64_PURGATORY_EXTRA_ASFLAGS += -mcall-aixdesc
else
	ppc64_PURGATORY_EXTRA_LDFLAGS += -melf64lppc
	ppc64_PURGATORY_EXTRA_CFLAGS += -mlittle-endian
	ppc64_PURGATORY_EXTRA_ASFLAGS += -mlittle-endian
endif

dist += purgatory/arch/ppc64/Makefile $(ppc64_PURGATORY_SRCS)		\
	purgatory/arch/ppc64/hvCall.h					\
	purgatory/arch/ppc64/ppc64_asm.h				\
	purgatory/arch/ppc64/purgatory-ppc64.h
