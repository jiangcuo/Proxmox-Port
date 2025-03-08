/*
 * Purgatory setup code
 *
 * Copyright IBM Corp. 2011
 *
 * Author(s): Michael Holzheu <holzheu@linux.vnet.ibm.com>
 */

	.text
	.globl purgatory_start
	.balign 16
purgatory_start:
#ifdef __s390x__
	larl	%r5,gprs_save_area
	stmg	%r6,%r15,0(%r5)
	larl	%r15,lstack_end
	aghi	%r15,-160

	cghi	%r2,0
	je	verify_checksums

	brasl	%r14,purgatory
	larl	%r14,kdump_psw
	lpswe	0(%r14)

verify_checksums:
	brasl	%r14,verify_sha256_digest
	larl	%r5,gprs_save_area
	lmg	%r6,%r15,0(%r5)
	br	%r14
	.section ".data"
	.balign 16
kdump_psw:
	.quad 0x0000000180000000
	.quad 0x0000000000010010

	.bss
gprs_save_area:
	.fill 80

	.balign 4096
lstack:
	.skip 4096
lstack_end:
#else
0:	j	0
#endif
