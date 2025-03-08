/* Base Address */
#define TTYS0_BASE	0x3f8
/* Data */
#define TTYS0_RBR (TTYS0_BASE+0x00)
#define TTYS0_TBR (TTYS0_BASE+0x00)
/* Control */
#define TTYS0_IER (TTYS0_BASE+0x01)
#define TTYS0_IIR (TTYS0_BASE+0x02)
#define TTYS0_FCR (TTYS0_BASE+0x02)
#define TTYS0_LCR (TTYS0_BASE+0x03)
#define TTYS0_MCR (TTYS0_BASE+0x04)

#define TTYS0_DLL (TTYS0_BASE+0x00)
#define TTYS0_DLM (TTYS0_BASE+0x01)
/* Status */
#define TTYS0_LSR (TTYS0_BASE+0x05)
#define TTYS0_MSR (TTYS0_BASE+0x06)
#define TTYS0_SCR (TTYS0_BASE+0x07)

#define TTYS0_BAUD 9600
#define TTYS0_DIV  (115200/TTYS0_BAUD)
#define TTYS0_DIV_LO	(TTYS0_DIV&0xFF)
#define TTYS0_DIV_HI	((TTYS0_DIV >> 8)&0xFF)

#if ((115200%TTYS0_BAUD) != 0)
#error Bad ttyS0 baud rate
#endif

#define TTYS0_INIT	\
	/* disable interrupts */	\
	movb	$0x00, %al		; \
	movw	$TTYS0_IER, %dx		; \
	outb	%al, %dx		; \
					; \
	/* enable fifos */		\
	movb	$0x01, %al		; \
	movw	$TTYS0_FCR, %dx		; \
	outb	%al, %dx		; \
					; \
	/* Set Baud Rate Divisor to TTYS0_BAUD */	\
	movw	$TTYS0_LCR, %dx		; \
	movb	$0x83, %al		; \
	outb	%al, %dx		; \
					; \
	movw	$TTYS0_DLL, %dx		; \
	movb	$TTYS0_DIV_LO, %al	; \
	outb	%al, %dx		; \
					; \
	movw	$TTYS0_DLM, %dx		; \
	movb	$TTYS0_DIV_HI, %al	; \
	outb	%al, %dx		; \
					; \
	movw	$TTYS0_LCR, %dx		; \
	movb	$0x03, %al		; \
	outb	%al, %dx
	

	/* uses:	ax, dx */
#define TTYS0_TX_AL		\
	mov	%al, %ah	; \
9:	mov	$TTYS0_LSR, %dx	; \
	inb	%dx, %al	; \
	test	$0x20, %al	; \
	je	9b		; \
	mov	$TTYS0_TBR, %dx	; \
	mov	%ah, %al	; \
	outb	%al, %dx

	/* uses:	ax, dx */
#define TTYS0_TX_CHAR(byte)	\
	mov	byte, %al	; \
	TTYS0_TX_AL

	/* uses:	eax, dx */
#define TTYS0_TX_HEX32(lword)	\
	mov	lword, %eax	; \
	shr	$28, %eax	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %eax	; \
	shr	$24, %eax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %eax	; \
	shr	$20, %eax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %eax	; \
	shr	$16, %eax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %eax	; \
	shr	$12, %eax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %eax	; \
	shr	$8, %eax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %eax	; \
	shr	$4, %eax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %eax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL

	/* uses:	rax, dx */
#define TTYS0_TX_HEX64(lword)	\
	mov	lword, %rax	; \
	shr	$60, %rax	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$56, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$52, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$48, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$44, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$40, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$36, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$32, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$28, %rax	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$24, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$20, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$16, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$12, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$8, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	shr	$4, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL		; \
				; \
	mov	lword, %rax	; \
	and	$0x0f, %al	; \
	add	$'0', %al	; \
	cmp	$'9', %al	; \
	jle	9f		; \
	add	$39, %al	; \
9:				; \
	TTYS0_TX_AL
	

#define DEBUG_CHAR(x) TTYS0_TX_CHAR($x) ;  TTYS0_TX_CHAR($'\r') ;  TTYS0_TX_CHAR($'\n')
#define DEBUG_TX_HEX32(x) TTYS0_TX_HEX32(x); TTYS0_TX_CHAR($'\r') ;  TTYS0_TX_CHAR($'\n')
#define DEBUG_TX_HEX64(x) TTYS0_TX_HEX64(x); TTYS0_TX_CHAR($'\r') ;  TTYS0_TX_CHAR($'\n')

