#ifndef KEXEC_MIPS_H
#define KEXEC_MIPS_H

#include <sys/types.h>

#define BOOT_BLOCK_VERSION 17
#define BOOT_BLOCK_LAST_COMP_VERSION 16

#define MAX_MEMORY_RANGES  64
#define MAX_LINE          160

#define CORE_TYPE_ELF32 1
#define CORE_TYPE_ELF64 2

int elf_mips_probe(const char *buf, off_t len);
int elf_mips_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void elf_mips_usage(void);

struct arch_options_t {
	char *command_line;
	char *dtb_file;
	char *initrd_file;
	int core_header_type;
};

extern struct memory_ranges usablemem_rgns;
extern off_t initrd_base, initrd_size;

#endif /* KEXEC_MIPS_H */
