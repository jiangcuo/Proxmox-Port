#ifndef KEXEC_IA64_H
#define KEXEC_IA64_H

extern int max_memory_ranges;
int elf_ia64_probe(const char *buf, off_t len);
int elf_ia64_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void elf_ia64_usage(void);
int update_loaded_segments(struct mem_ehdr *ehdr);
void move_loaded_segments(struct mem_ehdr *ehdr, unsigned long addr);

#define EFI_PAGE_SIZE	  (1UL<<12)
#define ELF_PAGE_SIZE	  (1UL<<16)
#endif /* KEXEC_IA64_H */
