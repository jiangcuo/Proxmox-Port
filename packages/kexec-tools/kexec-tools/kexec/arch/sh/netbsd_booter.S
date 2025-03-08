	.globl	netbsd_booter
netbsd_booter:
	mov.l	ccr,r0
	mov	#0,r1
	mov.l	r1,@r0
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	mova	netbsd_start,r0
	mov.l	@r0,r1
	add	#4,r0
	mov.l	@r0,r4		! howto
	add	#4,r0
	mov	r0,r5		! bootinfo
	jmp	@r1
	nop
	nop
	nop
	.align 4
ccr:	.long 0xff00001c

	.align 8
netbsd_start:
	.long 0x8c001000
	.long 0x200		! howto
	.long 0			! bootinfo
	.long 0			! bootinfo
	.long 0			! bootinfo
	.long 0			! bootinfo
	.long 0			! bootinfo
	.long 0			! bootinfo
	.long 0			! bootinfo
	.long 0			! bootinfo
	.long 0			! bootinfo
	.long 0			! bootinfo
	.long 0			! bootinfo
	.long 0			! bootinfo
	.long 0			! bootinfo
	.long 0			! bootinfo

