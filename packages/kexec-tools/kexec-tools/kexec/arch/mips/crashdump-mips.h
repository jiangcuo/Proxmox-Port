#ifndef CRASHDUMP_MIPS_H
#define CRASHDUMP_MIPS_H

struct kexec_info;
int load_crashdump_segments(struct kexec_info *info, char *mod_cmdline,
				unsigned long max_addr, unsigned long min_base);
#ifdef __mips64
#define PAGE_OFFSET	0xa800000000000000ULL
#define MAXMEM		0
#else
#define PAGE_OFFSET	0x80000000
#define MAXMEM		0x80000000
#endif
#define __pa(x)		((unsigned long)(X) & 0x7fffffff)

#define LOONGSON_PAGE_OFFSET	0xffffffff80000000ULL
#define OCTEON_PAGE_OFFSET	0x8000000000000000ULL

#define CRASH_MAX_MEMMAP_NR	(KEXEC_MAX_SEGMENTS + 1)
#define CRASH_MAX_MEMORY_RANGES	(MAX_MEMORY_RANGES + 2)

#define COMMAND_LINE_SIZE	512

/* Backup Region, First 1M of System RAM. */
#define BACKUP_SRC_START	0x00000000
#define BACKUP_SRC_END		0x000fffff
#define BACKUP_SRC_SIZE	(BACKUP_SRC_END - BACKUP_SRC_START + 1)

extern struct arch_options_t arch_options;
#endif /* CRASHDUMP_MIPS_H */
