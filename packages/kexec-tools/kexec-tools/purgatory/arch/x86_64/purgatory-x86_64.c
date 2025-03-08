#include <stdint.h>
#include <stddef.h>
#include <purgatory.h>
#include "purgatory-x86_64.h"

uint8_t reset_vga = 0;
uint8_t legacy_pic = 0;
uint8_t panic_kernel = 0;
unsigned long jump_back_entry = 0;
char *cmdline_end = NULL;

void setup_arch(void)
{
	if (reset_vga)    x86_reset_vga();
	if (legacy_pic)   x86_setup_legacy_pic();
}

void x86_setup_jump_back_entry(void)
{
	if (cmdline_end)
		sprintf(cmdline_end, " kexec_jump_back_entry=0x%lx",
			jump_back_entry);
}

/* This function can be used to execute after the SHA256 verification. */
void post_verification_setup_arch(void)
{
	if (panic_kernel)    crashdump_backup_memory();
	if (jump_back_entry) x86_setup_jump_back_entry();
}
