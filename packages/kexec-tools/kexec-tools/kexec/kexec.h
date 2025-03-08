#ifndef KEXEC_H
#define KEXEC_H

#include "config.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define USE_BSD
#include <byteswap.h>
#include <endian.h>
#define _GNU_SOURCE

#include "kexec-elf.h"
#include "unused.h"

#ifndef BYTE_ORDER
#error BYTE_ORDER not defined
#endif

#ifndef LITTLE_ENDIAN
#error LITTLE_ENDIAN not defined
#endif

#ifndef BIG_ENDIAN
#error BIG_ENDIAN not defined
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#define cpu_to_le16(val) (val)
#define cpu_to_le32(val) (val)
#define cpu_to_le64(val) (val)
#define cpu_to_be16(val) bswap_16(val)
#define cpu_to_be32(val) bswap_32(val)
#define cpu_to_be64(val) bswap_64(val)
#define le16_to_cpu(val) (val)
#define le32_to_cpu(val) (val)
#define le64_to_cpu(val) (val)
#define be16_to_cpu(val) bswap_16(val)
#define be32_to_cpu(val) bswap_32(val)
#define be64_to_cpu(val) bswap_64(val)
#elif BYTE_ORDER == BIG_ENDIAN
#define cpu_to_le16(val) bswap_16(val)
#define cpu_to_le32(val) bswap_32(val)
#define cpu_to_le64(val) bswap_64(val)
#define cpu_to_be16(val) (val)
#define cpu_to_be32(val) (val)
#define cpu_to_be64(val) (val)
#define le16_to_cpu(val) bswap_16(val)
#define le32_to_cpu(val) bswap_32(val)
#define le64_to_cpu(val) bswap_64(val)
#define be16_to_cpu(val) (val)
#define be32_to_cpu(val) (val)
#define be64_to_cpu(val) (val)
#else
#error unknwon BYTE_ORDER
#endif

/*
 * Document some of the reasons why crashdump may fail, so we can give
 * better error messages
 */
#define EFAILED		-1	/* default error code */
#define ENOCRASHKERNEL	-2	/* no memory reserved for crashkernel */
#define EFALLBACK	-3	/* fallback to kexec_load(2) may work */

/*
 * This function doesn't actually exist.  The idea is that when someone
 * uses the macros below with an unsupported size (datatype), the linker
 * will alert us to the problem via an unresolved reference error.
 */
extern unsigned long bad_unaligned_access_length (void);

#define get_unaligned(loc) \
({ \
	__typeof__(*(loc)) _v; \
	size_t size = sizeof(*(loc)); \
	switch(size) {  \
	case 1: case 2: case 4: case 8: \
		memcpy(&_v, (loc), size); \
		break; \
	default: \
		_v = bad_unaligned_access_length(); \
		break; \
	} \
	_v; \
})

#define put_unaligned(value, loc) \
do { \
	size_t size = sizeof(*(loc)); \
	__typeof__(*(loc)) _v = value; \
	switch(size) { \
	case 1: case 2: case 4: case 8: \
		memcpy((loc), &_v, size); \
		break; \
	default: \
		bad_unaligned_access_length(); \
		break; \
	} \
} while(0)

#define _ALIGN_UP_MASK(addr, mask)   (((addr) + (mask)) & ~(mask))
#define _ALIGN_DOWN_MASK(addr, mask) ((addr) & ~(mask))

/* align addr on a size boundary - adjust address up/down if needed */
#define _ALIGN_UP(addr, size)	     \
	_ALIGN_UP_MASK(addr, (typeof(addr))(size) - 1)
#define _ALIGN_DOWN(addr, size)	     \
	_ALIGN_DOWN_MASK(addr, (typeof(addr))(size) - 1)

/* align addr on a size boundary - adjust address up if needed */
#define _ALIGN(addr, size)     _ALIGN_UP(addr, size)

extern unsigned long long mem_min, mem_max;
extern int kexec_debug;

#define dbgprintf(...) \
do { \
	if (kexec_debug) \
		fprintf(stderr, __VA_ARGS__); \
} while(0)

struct kexec_segment {
	const void *buf;
	size_t bufsz;
	const void *mem;
	size_t memsz;
};

struct memory_range {
	unsigned long long start, end;
	unsigned type;
#define RANGE_RAM	0
#define RANGE_RESERVED	1
#define RANGE_ACPI	2
#define RANGE_ACPI_NVS	3
#define RANGE_UNCACHED	4
#define RANGE_PMEM		6
#define RANGE_PRAM		11
};

struct memory_ranges {
        unsigned int size;
	unsigned int max_size;
        struct memory_range *ranges;
};

struct kexec_info {
	struct kexec_segment *segment;
	int nr_segments;
	struct memory_range *memory_range;
	int memory_ranges;
	struct memory_range *crash_range;
	int nr_crash_ranges;
	void *entry;
	struct mem_ehdr rhdr;
	unsigned long backup_start;
	unsigned long kexec_flags;
	unsigned long backup_src_start;
	unsigned long backup_src_size;
	/* Set to 1 if we are using kexec file syscall */
	unsigned long file_mode :1;

	/* Filled by kernel image processing code */
	int kernel_fd;
	int initrd_fd;
	char *command_line;
	int command_line_len;

	int skip_checks;
	unsigned long elfcorehdr;
};

struct arch_map_entry {
	const char *machine;
	unsigned long arch;
};

extern const struct arch_map_entry arches[];
long physical_arch(void);

void usage(void);
int get_memory_ranges(struct memory_range **range, int *ranges,
						unsigned long kexec_flags);
int valid_memory_range(struct kexec_info *info,
		       unsigned long sstart, unsigned long send);
void print_segments(FILE *file, struct kexec_info *info);
int sort_segments(struct kexec_info *info);
unsigned long locate_hole(struct kexec_info *info,
	unsigned long hole_size, unsigned long hole_align, 
	unsigned long hole_min, unsigned long hole_max,
	int hole_end);

typedef int (probe_t)(const char *kernel_buf, off_t kernel_size);
typedef int (load_t )(int argc, char **argv,
	const char *kernel_buf, off_t kernel_size, 
	struct kexec_info *info);
typedef void (usage_t)(void);
struct file_type {
	const char *name;
	probe_t *probe;
	load_t  *load;
	usage_t *usage;
};

extern struct file_type file_type[];
extern int file_types;

#define OPT_HELP		'h'
#define OPT_VERSION		'v'
#define OPT_DEBUG		'd'
#define OPT_FORCE		'f'
#define OPT_NOCHECKS		'i'
#define OPT_NOIFDOWN		'x'
#define OPT_NOSYNC		'y'
#define OPT_EXEC		'e'
#define OPT_LOAD		'l'
#define OPT_UNLOAD		'u'
#define OPT_TYPE		't'
#define OPT_PANIC		'p'
#define OPT_KEXEC_FILE_SYSCALL	's'
#define OPT_KEXEC_SYSCALL	'c'
#define OPT_KEXEC_SYSCALL_AUTO	'a'
#define OPT_STATUS		'S'
#define OPT_MEM_MIN             256
#define OPT_MEM_MAX             257
#define OPT_REUSE_INITRD	258
#define OPT_LOAD_PRESERVE_CONTEXT 259
#define OPT_LOAD_JUMP_BACK_HELPER 260
#define OPT_ENTRY		261
#define OPT_PRINT_CKR_SIZE	262
#define OPT_LOAD_LIVE_UPDATE	263
#define OPT_EXEC_LIVE_UPDATE	264
#define OPT_HOTPLUG		        265
#define OPT_MAX		266
#define KEXEC_OPTIONS \
	{ "help",		0, 0, OPT_HELP }, \
	{ "version",		0, 0, OPT_VERSION }, \
	{ "force",		0, 0, OPT_FORCE }, \
	{ "no-checks",		0, 0, OPT_NOCHECKS }, \
	{ "no-ifdown",		0, 0, OPT_NOIFDOWN }, \
	{ "no-sync",		0, 0, OPT_NOSYNC }, \
	{ "load",		0, 0, OPT_LOAD }, \
	{ "unload",		0, 0, OPT_UNLOAD }, \
	{ "exec",		0, 0, OPT_EXEC }, \
	{ "exec-live-update",	0, 0, OPT_EXEC_LIVE_UPDATE}, \
	{ "load-preserve-context", 0, 0, OPT_LOAD_PRESERVE_CONTEXT}, \
	{ "load-jump-back-helper", 0, 0, OPT_LOAD_JUMP_BACK_HELPER }, \
	{ "load-live-update", 0, 0, OPT_LOAD_LIVE_UPDATE }, \
	{ "entry",		1, 0, OPT_ENTRY }, \
	{ "type",		1, 0, OPT_TYPE }, \
	{ "load-panic",         0, 0, OPT_PANIC }, \
	{ "mem-min",		1, 0, OPT_MEM_MIN }, \
	{ "mem-max",		1, 0, OPT_MEM_MAX }, \
	{ "reuseinitrd",	0, 0, OPT_REUSE_INITRD }, \
	{ "kexec-file-syscall",	0, 0, OPT_KEXEC_FILE_SYSCALL }, \
	{ "kexec-syscall",	0, 0, OPT_KEXEC_SYSCALL }, \
	{ "kexec-syscall-auto",	0, 0, OPT_KEXEC_SYSCALL_AUTO }, \
	{ "debug",		0, 0, OPT_DEBUG }, \
	{ "status",		0, 0, OPT_STATUS }, \
	{ "print-ckr-size",     0, 0, OPT_PRINT_CKR_SIZE }, \
	{ "hotplug",		    0, 0, OPT_HOTPLUG }, \

#define KEXEC_OPT_STR "h?vdfixyluet:pscaS"

extern void dbgprint_mem_range(const char *prefix, struct memory_range *mr, int nr_mr);
extern void die(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));
extern void *xmalloc(size_t size);
extern void *xrealloc(void *ptr, size_t size);
extern char *slurp_fd(int fd, const char *filename, off_t size, off_t *nread);
extern char *slurp_file(const char *filename, off_t *r_size);
extern char *slurp_file_mmap(const char *filename, off_t *r_size);
extern char *slurp_file_len(const char *filename, off_t size, off_t *nread);
extern char *slurp_decompress_file(const char *filename, off_t *r_size);
extern unsigned long virt_to_phys(unsigned long addr);
extern void add_segment(struct kexec_info *info,
	const void *buf, size_t bufsz, unsigned long base, size_t memsz);
extern void add_segment_phys_virt(struct kexec_info *info,
	const void *buf, size_t bufsz, unsigned long base, size_t memsz,
	int phys);
extern unsigned long add_buffer(struct kexec_info *info,
	const void *buf, unsigned long bufsz, unsigned long memsz,
	unsigned long buf_align, unsigned long buf_min, unsigned long buf_max,
	int buf_end);
extern unsigned long add_buffer_virt(struct kexec_info *info,
	const void *buf, unsigned long bufsz, unsigned long memsz,
	unsigned long buf_align, unsigned long buf_min, unsigned long buf_max,
	int buf_end);
extern unsigned long add_buffer_phys_virt(struct kexec_info *info,
	const void *buf, unsigned long bufsz, unsigned long memsz,
	unsigned long buf_align, unsigned long buf_min, unsigned long buf_max,
	int buf_end, int phys);
extern void arch_reuse_initrd(void);

extern int ifdown(void);

extern char purgatory[];
extern size_t purgatory_size;

extern unsigned long elfcorehdrsz;
extern int do_hotplug;

#define BOOTLOADER "kexec"
#define BOOTLOADER_VERSION PACKAGE_VERSION

void arch_usage(void);
/* Return non-zero if segment needs to be excluded from SHA calculation, else 0. */
int arch_do_exclude_segment(struct kexec_info *info, struct kexec_segment *segment);
int arch_process_options(int argc, char **argv);
int arch_compat_trampoline(struct kexec_info *info);
void arch_update_purgatory(struct kexec_info *info);
int is_crashkernel_mem_reserved(void);
int get_crash_kernel_load_range(uint64_t *start, uint64_t *end);
void remove_parameter(char *line, const char *param_name);
char *get_command_line(void);

int kexec_iomem_for_each_line(char *match,
			      int (*callback)(void *data,
					      int nr,
					      char *str,
					      unsigned long long base,
					      unsigned long long length),
			      void *data);
int parse_iomem_single(char *str, uint64_t *start, uint64_t *end);
const char * proc_iomem(void);

#define MAX_LINE	160

char *concat_cmdline(const char *base, const char *append);
void cmdline_add_liveupdate(char **base);

int xen_present(void);
int xen_kexec_load(struct kexec_info *info);
int xen_kexec_unload(uint64_t kexec_flags);
int xen_kexec_exec(uint64_t kexec_flags);
int xen_kexec_status(uint64_t kexec_flags);

extern unsigned long long get_kernel_sym(const char *text);

/* Converts unsigned long to ascii string. */
static inline void ultoa(unsigned long val, char *str)
{
	char buf[36];
	int len = 0, pos = 0;

	do {
		buf[len++] = val % 10;
		val /= 10;
	} while (val);

	while (len)
		str[pos++] = buf[--len] + '0';
	str[pos] = 0;
}

#endif /* KEXEC_H */
