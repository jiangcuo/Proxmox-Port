/*
 * ARM64 purgatory.
 */

.macro	size, sym:req
	.size \sym, . - \sym
.endm

.text

.globl purgatory_start
purgatory_start:

	adr	x19, .Lstack
	mov	sp, x19

	bl	purgatory

	/* Start new image. */
	ldr	x17, arm64_kernel_entry
	ldr	x0, arm64_dtb_addr
	mov	x1, xzr
	mov	x2, xzr
	mov	x3, xzr
	br	x17

size purgatory_start

.ltorg

.align 4
	.rept	256
	.quad	0
	.endr
.Lstack:

.data

.align 3

.globl arm64_kernel_entry
arm64_kernel_entry:
	.quad	0
size arm64_kernel_entry

.globl arm64_dtb_addr
arm64_dtb_addr:
	.quad	0
size arm64_dtb_addr

.end