/*
 * ARM64 kexec.
 */

#if !defined(KEXEC_ARM64_H)
#define KEXEC_ARM64_H

#include <stdbool.h>
#include <sys/types.h>

#include "image-header.h"
#include "kexec.h"

#define KEXEC_SEGMENT_MAX 64

#define BOOT_BLOCK_VERSION 17
#define BOOT_BLOCK_LAST_COMP_VERSION 16
#define COMMAND_LINE_SIZE 2048 /* from kernel */

#define KiB(x) ((x) * 1024UL)
#define MiB(x) (KiB(x) * 1024UL)
#define GiB(x) (MiB(x) * 1024UL)

#define ULONGLONG_MAX	(~0ULL)

/*
 * Incorrect address
 */
#define NOT_KV_ADDR	(0x0)
#define NOT_PADDR	(ULONGLONG_MAX)

int elf_arm64_probe(const char *kernel_buf, off_t kernel_size);
int elf_arm64_load(int argc, char **argv, const char *kernel_buf,
	off_t kernel_size, struct kexec_info *info);
void elf_arm64_usage(void);

int image_arm64_probe(const char *kernel_buf, off_t kernel_size);
int image_arm64_load(int argc, char **argv, const char *kernel_buf,
	off_t kernel_size, struct kexec_info *info);
void image_arm64_usage(void);

int uImage_arm64_probe(const char *buf, off_t len);
int uImage_arm64_load(int argc, char **argv, const char *buf, off_t len,
		      struct kexec_info *info);
void uImage_arm64_usage(void);

int pez_arm64_probe(const char *kernel_buf, off_t kernel_size);
int pez_arm64_load(int argc, char **argv, const char *buf, off_t len,
			struct kexec_info *info);
void pez_arm64_usage(void);


extern off_t initrd_base;
extern off_t initrd_size;

/**
 * struct arm64_mem - Memory layout info.
 */

struct arm64_mem {
	int64_t phys_offset;
	uint64_t text_offset;
	uint64_t image_size;
	uint64_t vp_offset;
};

#define arm64_mem_ngv UINT64_MAX
extern struct arm64_mem arm64_mem;

uint64_t get_phys_offset(void);
uint64_t get_vp_offset(void);
int get_page_offset(unsigned long *offset);

static inline void reset_vp_offset(void)
{
	arm64_mem.vp_offset = arm64_mem_ngv;
}

int arm64_process_image_header(const struct arm64_image_header *h);
unsigned long arm64_locate_kernel_segment(struct kexec_info *info);
int arm64_load_other_segments(struct kexec_info *info,
	unsigned long image_base);

#endif
