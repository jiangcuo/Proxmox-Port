#ifndef KEXEC_CRIS_H
#define KEXEC_CRIS_H

int elf_cris_probe(const char *buf, off_t len);
int elf_cris_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void elf_cris_usage(void);

#endif /* KEXEC_CRIS_H */
