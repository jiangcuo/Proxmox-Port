#ifndef KEXEC_X86_H
#define KEXEC_X86_H

#define MAX_MEMORY_RANGES 2048

enum coretype {
	CORE_TYPE_UNDEF = 0,
	CORE_TYPE_ELF32 = 1,
	CORE_TYPE_ELF64 = 2
};

extern unsigned char compat_x86_64[];
extern uint32_t compat_x86_64_size, compat_x86_64_entry32;

struct entry32_regs {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t esp;
	uint32_t ebp;
	uint32_t eip;
};

struct entry16_regs {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t esp;
	uint32_t ebp;
	uint16_t ds;
	uint16_t es;
	uint16_t ss;
	uint16_t fs;
	uint16_t gs;
	uint16_t ip;
	uint16_t cs;
	uint16_t pad;
};

struct arch_options_t {
	uint8_t  	reset_vga;
	uint16_t 	serial_base;
	uint32_t 	serial_baud;
	uint8_t  	console_vga;
	uint8_t  	console_serial;
	enum coretype	core_header_type;
	uint8_t  	pass_memmap_cmdline;
	uint8_t		noefi;
	uint8_t		reuse_video_type;
};

int multiboot_x86_probe(const char *buf, off_t len);
int multiboot_x86_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void multiboot_x86_usage(void);

int multiboot2_x86_load(int argc, char **argv, const char *buf, off_t len,
			struct kexec_info *info);
void multiboot2_x86_usage(void);
int multiboot2_x86_probe(const char *buf, off_t buf_len);

int elf_x86_probe(const char *buf, off_t len);
int elf_x86_any_probe(const char *buf, off_t len, enum coretype arch);
int elf_x86_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void elf_x86_usage(void);

int bzImage_probe(const char *buf, off_t len);
int bzImage_load(int argc, char **argv, const char *buf, off_t len, 
	struct kexec_info *info);
void bzImage_usage(void);
int do_bzImage_load(struct kexec_info *info,
	const char *kernel, off_t kernel_len,
	const char *command_line, off_t command_line_len,
	const char *initrd, off_t initrd_len,
	const char *dtb, off_t dtb_len,
	int real_mode_entry);

int beoboot_probe(const char *buf, off_t len);
int beoboot_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void beoboot_usage(void);

int nbi_probe(const char *buf, off_t len);
int nbi_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void nbi_usage(void);

extern unsigned xen_e820_to_kexec_type(uint32_t type);
extern uint64_t get_acpi_rsdp(void);
#endif /* KEXEC_X86_H */
