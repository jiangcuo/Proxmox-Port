
arm64_PURGATORY_EXTRA_CFLAGS = \
	-mcmodel=large \
	-fno-PIC \
	-fno-stack-protector \
	-fno-asynchronous-unwind-tables \
	-Wundef \
	-Werror-implicit-function-declaration \
	-Wdeclaration-after-statement \
	-Werror=implicit-int \
	-Werror=strict-prototypes

arm64_PURGATORY_SRCS += \
	purgatory/arch/arm64/entry.S \
	purgatory/arch/arm64/purgatory-arm64.c

dist += \
	$(arm64_PURGATORY_SRCS) \
	purgatory/arch/arm64/Makefile
