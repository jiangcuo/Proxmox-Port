#ifndef KEXEC_ELF_BOOT_H
#define KEXEC_ELF_BOOT_H

unsigned long elf_boot_notes(
	struct kexec_info *info, unsigned long max_addr, 
	const char *cmdline, int cmdline_len);

#endif /* KEXEC_ELF_BOOT_H */
