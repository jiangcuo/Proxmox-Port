/*
 * ARM64 purgatory.
 */

#include <stdint.h>
#include <purgatory.h>

/* Symbols set by kexec. */

uint8_t *arm64_sink __attribute__ ((section ("data")));
extern void (*arm64_kernel_entry)(uint64_t, uint64_t, uint64_t, uint64_t);
extern uint64_t arm64_dtb_addr;

void putchar(int ch)
{
	if (!arm64_sink)
		return;

	*arm64_sink = ch;

	if (ch == '\n')
		*arm64_sink = '\r';
}

void post_verification_setup_arch(void)
{
	printf("purgatory: booting kernel now\n");
}

void setup_arch(void)
{
	printf("purgatory: entry=%lx\n", (unsigned long)arm64_kernel_entry);
	printf("purgatory: dtb=%lx\n", arm64_dtb_addr);
}
