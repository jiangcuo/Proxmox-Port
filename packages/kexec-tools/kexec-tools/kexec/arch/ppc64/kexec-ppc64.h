#ifndef KEXEC_PPC64_H
#define KEXEC_PPC64_H

#define PATH_LEN 256
#define MAXBYTES 128
#define MAX_LINE 160
#define CORE_TYPE_ELF32 1
#define CORE_TYPE_ELF64 2

#define BOOT_BLOCK_VERSION 17
#define BOOT_BLOCK_LAST_COMP_VERSION 17
#if (BOOT_BLOCK_VERSION < 16)
#	define NEED_STRUCTURE_BLOCK_EXTRA_PAD
#endif
#define HAVE_DYNAMIC_MEMORY
#define NEED_RESERVE_DTB

extern int get_devtree_value(const char *fname, unsigned long long *pvalue);

int setup_memory_ranges(unsigned long kexec_flags);

int elf_ppc64_probe(const char *buf, off_t len);
int elf_ppc64_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void elf_ppc64_usage(void);

struct mem_ehdr;
unsigned long my_r2(const struct mem_ehdr *ehdr);

extern uint64_t initrd_base, initrd_size;
extern int max_memory_ranges;
extern unsigned char reuse_initrd;

struct arch_options_t {
	int core_header_type;
};

typedef struct mem_rgns {
        unsigned int size;
        struct memory_range *ranges;
} mem_rgns_t;

extern mem_rgns_t usablemem_rgns;

#endif /* KEXEC_PPC64_H */
