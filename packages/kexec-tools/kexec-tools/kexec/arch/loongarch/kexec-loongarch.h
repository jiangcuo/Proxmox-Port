#ifndef KEXEC_LOONGARCH_H
#define KEXEC_LOONGARCH_H

#include <sys/types.h>

#include "image-header.h"

#define BOOT_BLOCK_VERSION 17
#define BOOT_BLOCK_LAST_COMP_VERSION 16

#define MAX_MEMORY_RANGES 64
#define MAX_LINE 160

#define CORE_TYPE_ELF64 1

#define COMMAND_LINE_SIZE 512

#define KiB(x) ((x) * 1024UL)
#define MiB(x) (KiB(x) * 1024UL)

int elf_loongarch_probe(const char *kernel_buf, off_t kernel_size);
int elf_loongarch_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void elf_loongarch_usage(void);

int pei_loongarch_probe(const char *buf, off_t len);
int pei_loongarch_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void pei_loongarch_usage(void);
int pez_loongarch_probe(const char *kernel_buf, off_t kernel_size);
int pez_loongarch_load(int argc, char **argv, const char *buf, off_t len,
		       struct kexec_info *info);
void pez_loongarch_usage(void);

int loongarch_process_image_header(const struct loongarch_image_header *h);

unsigned long loongarch_locate_kernel_segment(struct kexec_info *info);
int loongarch_load_other_segments(struct kexec_info *info,
	unsigned long hole_min);

struct arch_options_t {
	char *command_line;
	char *initrd_file;
	char *dtb;
	int core_header_type;
};

/**
 * struct loongarch_mem - Memory layout info.
 */

struct loongarch_mem {
	uint64_t phys_offset;
	uint64_t text_offset;
	uint64_t image_size;
};

extern struct loongarch_mem loongarch_mem;

extern struct memory_ranges usablemem_rgns;
extern struct arch_options_t arch_options;
extern off_t initrd_base, initrd_size;

#endif /* KEXEC_LOONGARCH_H */
