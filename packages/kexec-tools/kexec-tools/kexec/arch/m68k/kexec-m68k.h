#ifndef KEXEC_M68K_H
#define KEXEC_M68K_H

int elf_m68k_probe(const char *buf, off_t len);
int elf_m68k_load(int argc, char **argv, const char *buf, off_t len,
		  struct kexec_info *info);
void elf_m68k_usage(void);

#endif /* KEXEC_M68K_H */
