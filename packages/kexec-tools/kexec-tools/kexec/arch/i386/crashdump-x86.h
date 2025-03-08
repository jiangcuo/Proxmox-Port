#ifndef CRASHDUMP_X86_H
#define CRASHDUMP_X86_H

struct kexec_info;
int load_crashdump_segments(struct kexec_info *info, char *mod_cmdline,
				unsigned long max_addr, unsigned long min_base);

#define X86_PAGE_OFFSET	0xc0000000
#define x86__pa(x)		((unsigned long)(x)-X86_PAGE_OFFSET)

#define X86__VMALLOC_RESERVE       (128 << 20)
#define X86_MAXMEM                  (-X86_PAGE_OFFSET-X86__VMALLOC_RESERVE)

#define X86_64__START_KERNEL_map	0xffffffff80000000ULL
#define X86_64_PAGE_OFFSET_PRE_2_6_27	0xffff810000000000ULL
#define X86_64_PAGE_OFFSET_PRE_4_20_0	0xffff880000000000ULL
#define X86_64_PAGE_OFFSET	0xffff888000000000ULL

#define X86_64_MAXMEM        		0x3fffffffffffUL

/* Kernel text size */
#define X86_64_KERNEL_TEXT_SIZE  (512UL*1024*1024)

#define CRASH_MAX_MEMMAP_NR	1024

#define CRASH_MAX_MEMORY_RANGES	32768

/* Backup Region, First 640K of System RAM. */
#define BACKUP_SRC_START	0x00000000
#define BACKUP_SRC_END		0x0009ffff
#define BACKUP_SRC_SIZE	(BACKUP_SRC_END - BACKUP_SRC_START + 1)

#endif /* CRASHDUMP_X86_H */
