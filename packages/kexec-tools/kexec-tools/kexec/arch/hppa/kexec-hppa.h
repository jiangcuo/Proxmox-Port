#ifndef KEXEC_HPPA_H
#define KEXEC_HPPA_H

int elf_hppa_probe(const char *buf, off_t len);
int elf_hppa_load(int argc, char **argv, const char *buf, off_t len,
		  struct kexec_info *info);
void elf_hppa_usage(void);

#endif /* KEXEC_HPPA_H */
