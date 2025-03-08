#include <stdint.h>
#include <purgatory.h>
#include "purgatory-x86.h"

/*
 * CPU
 * =============================================================================
 */

void x86_setup_cpu(void)
{
#if 0
	/* This code is only needed for old versions of the kexec kernel patch.
	 * While it is still a good idea doing this unconditionally breaks
	 * on older cpus that did not implemented cr4.
	 * So this code is disabled for now.  If this is revisited 
	 * I first need to detect cpuid support and then use cpuid 
	 * to conditionally change newer cpu registers.
	 */
	/* clear special bits in %cr4 */
	asm volatile(
		"movl	%0, %%eax\n\t"
		"movl	%%eax, %%cr4\n\t"
		: /* outputs */
		: "r" (0)
		);
#endif	
}

uint8_t reset_vga = 0;
uint8_t legacy_timer = 0;
uint8_t legacy_pic   = 0;
uint8_t panic_kernel = 0;
unsigned long jump_back_entry = 0;
char *cmdline_end = 0;

void setup_arch(void)
{
	x86_setup_cpu();
	if (reset_vga)    x86_reset_vga();
	if (legacy_pic)   x86_setup_legacy_pic();
	/* if (legacy_timer) x86_setup_legacy_timer(); */
}

static void x86_setup_jump_back_entry(void)
{
	if (cmdline_end)
		sprintf(cmdline_end, " kexec_jump_back_entry=0x%x",
			jump_back_entry);
}

/* This function can be used to execute after the SHA256 verification. */
void post_verification_setup_arch(void)
{
	if (panic_kernel)    crashdump_backup_memory();
	if (jump_back_entry) x86_setup_jump_back_entry();
}
