#ifndef CRASHDUMP_IA64_H
#define CRASHDUMP_IA64_H

#define PAGE_OFFSET             0xe000000000000000UL
#define __pa(x)			((unsigned long)(x)-PAGE_OFFSET)
extern int load_crashdump_segments(struct kexec_info *info,
		struct mem_ehdr *ehdr, unsigned long max_addr,
		unsigned long min_base, const char **cmdline);

#define CRASH_MAX_MEMMAP_NR     (KEXEC_MAX_SEGMENTS + 1)

#endif
