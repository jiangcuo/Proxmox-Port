#ifndef CRASHDUMP_ARM_H
#define CRASHDUMP_ARM_H

#ifdef __cplusplus
extern "C" {
#endif

#define COMMAND_LINE_SIZE	1024
#define DEFAULT_PAGE_OFFSET		(0xc0000000)
#define KVBASE_MASK	(0x1ffffff)
#define CRASH_MAX_MEMORY_RANGES	32
#define ARM_MAX_VIRTUAL		UINT32_MAX


extern struct memory_ranges usablemem_rgns;
extern struct memory_range crash_kernel_mem;
extern struct memory_range elfcorehdr_mem;

struct kexec_info;

extern int load_crashdump_segments(struct kexec_info *, char *);

#ifdef __cplusplus
}
#endif

#endif /* CRASHDUMP_ARM_H */
