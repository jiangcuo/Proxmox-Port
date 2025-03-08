#ifndef CRASHDUMP_LOONGARCH_H
#define CRASHDUMP_LOONGARCH_H

struct kexec_info;
extern struct memory_ranges usablemem_rgns;
extern struct memory_range crash_reserved_mem[];
extern struct memory_range elfcorehdr_mem;

int load_crashdump_segments(struct kexec_info *info);
int is_crashkernel_mem_reserved(void);
void fixup_elf_addrs(struct mem_ehdr *ehdr);
int get_crash_kernel_load_range(uint64_t *start, uint64_t *end);

#define PAGE_OFFSET	0x9000000000000000ULL
#define MAXMEM		0

#define CRASH_MAX_MEMMAP_NR	(KEXEC_MAX_SEGMENTS + 1)
#define CRASH_MAX_MEMORY_RANGES	(MAX_MEMORY_RANGES + 2)

/* crash dump kernel support at most two regions, low_region and high region. */
#define CRASH_MAX_RESERVED_RANGES      2

#define COMMAND_LINE_SIZE	512

extern struct arch_options_t arch_options;
#endif /* CRASHDUMP_LOONGARCH_H */
