#ifndef KEXEC_X86_64_H
#define KEXEC_X86_64_H

#include "../i386/kexec-x86.h"

struct entry64_regs {
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rip;
};

int elf_x86_64_probe(const char *buf, off_t len);
int elf_x86_64_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void elf_x86_64_usage(void);

int bzImage64_probe(const char *buf, off_t len);
int bzImage64_load(int argc, char **argv, const char *buf, off_t len,
			struct kexec_info *info);
void bzImage64_usage(void);

int multiboot2_x86_load(int argc, char **argv, const char *buf, off_t len,
			struct kexec_info *info);
void multiboot2_x86_usage(void);
int multiboot2_x86_probe(const char *buf, off_t buf_len);

#endif /* KEXEC_X86_64_H */
