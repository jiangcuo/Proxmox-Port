/*
 * S390 purgatory
 *
 * Copyright IBM Corp. 2011
 *
 * Author(s): Michael Holzheu <holzheu@linux.vnet.ibm.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../../../kexec/kexec-sha256.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

extern struct sha256_region sha256_regions[SHA256_REGIONS];

unsigned long crash_base = (unsigned long) -1;
unsigned long crash_size = (unsigned long) -1;

/*
 * Implement memcpy using the mvcle instruction
 */
static void memcpy_fast(void *target, void *src, unsigned long size)
{
	register unsigned long __target asm("2") = (unsigned long) target;
	register unsigned long __size1 asm("3") = size;
	register unsigned long __src asm("4") = (unsigned long) src;
	register unsigned long __size2 asm("5") = size;

	asm volatile (
		"0:	mvcle	%0,%2,0\n"
		"	jo	0b\n"
		: "+d" (__target), "+d" (__size1), "+d" (__src), "+d" (__size2)
		:
		: "cc", "memory"
	);
}

/*
 * Swap memory areas
 */
static void memswap(void *addr1, void *addr2, unsigned long size)
{
	unsigned long off, copy_len;
	static char buf[1024];

	for (off = 0; off < size; off += sizeof(buf)) {
		copy_len = MIN(size - off, sizeof(buf));
		memcpy_fast(buf, (void *) addr2 + off, copy_len);
		memcpy_fast(addr2 + off, addr1 + off, copy_len);
		memcpy_fast(addr1 + off, buf, copy_len);
	}
}

/*
 * Nothing to do
 */
void setup_arch(void)
{
}

/*
 * Do swap of [crash base - crash base + size] with [0 - crash size]
 *
 * We swap all kexec segments except of purgatory. The rest is copied
 * from [0 - crash size] to [crash base - crash base + size].
 * We use [0x2000 - 0x10000] for purgatory. This area is never used
 * by s390 Linux kernels.
 *
 * This functions assumes that the sha256_regions[] is sorted.
 */
void post_verification_setup_arch(void)
{
	unsigned long start, len, last = crash_base + 0x10000;
	struct sha256_region *ptr, *end;

	end = &sha256_regions[sizeof(sha256_regions)/sizeof(sha256_regions[0])];
	for (ptr = sha256_regions; ptr < end; ptr++) {
		if (!ptr->start)
			continue;
		start = MAX(ptr->start, crash_base + 0x10000);
		len = ptr->len - (start - ptr->start);
		memcpy_fast((void *) last, (void *) last - crash_base,
			    start - last);
		memswap((void *) start - crash_base, (void *) start, len);
		last = start + len;
	}
	memcpy_fast((void *) last, (void *) last - crash_base,
		    crash_base + crash_size - last);
	memcpy_fast((void *) crash_base, (void *) 0, 0x2000);
}
