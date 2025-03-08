#
# Purgatory ppc
#

ppc_PURGATORY_SRCS += purgatory/arch/ppc/v2wrap_32.S
ppc_PURGATORY_SRCS += purgatory/arch/ppc/misc.S
ppc_PURGATORY_SRCS += purgatory/arch/ppc/purgatory-ppc.c
ppc_PURGATORY_SRCS += purgatory/arch/ppc/console-ppc.c

ppc_PURGATORY_EXTRA_CFLAGS += -msoft-float

dist += purgatory/arch/ppc/Makefile $(ppc_PURGATORY_SRCS)		\
	purgatory/arch/ppc/purgatory-ppc.h purgatory/arch/ppc/ppc_asm.h

