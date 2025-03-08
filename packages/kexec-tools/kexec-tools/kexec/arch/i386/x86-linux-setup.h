#ifndef X86_LINUX_SETUP_H
#define X86_LINUX_SETUP_H
#include <x86/x86-linux.h>

void init_linux_parameters(struct x86_linux_param_header *real_mode);
void setup_linux_bootloader_parameters_high(
	struct kexec_info *info, struct x86_linux_param_header *real_mode,
	unsigned long real_mode_base, unsigned long cmdline_offset,
	const char *cmdline, off_t cmdline_len,
	const char *initrd_buf, off_t initrd_size, int initrd_high);
static inline void setup_linux_bootloader_parameters(
	struct kexec_info *info, struct x86_linux_param_header *real_mode,
	unsigned long real_mode_base, unsigned long cmdline_offset,
	const char *cmdline, off_t cmdline_len,
	const char *initrd_buf, off_t initrd_size)
{
	setup_linux_bootloader_parameters_high(info,
			real_mode, real_mode_base,
			cmdline_offset, cmdline, cmdline_len,
			initrd_buf, initrd_size, 0);
}
void setup_linux_system_parameters(struct kexec_info *info,
	struct x86_linux_param_header *real_mode);
void setup_linux_dtb(struct kexec_info *info, struct x86_linux_param_header *real_mode,
				   const char *dtb_buf, int dtb_len);
int get_bootparam(void *buf, off_t offset, size_t size);


#define SETUP_BASE    0x90000
#define KERN32_BASE  0x100000 /* 1MB */
#define INITRD_BASE 0x1000000 /* 16MB */

/* command line parameter may be appended by purgatory */
#define PURGATORY_CMDLINE_SIZE 64
extern int bzImage_support_efi_boot;
extern struct arch_options_t arch_options;

#endif /* X86_LINUX_SETUP_H */
