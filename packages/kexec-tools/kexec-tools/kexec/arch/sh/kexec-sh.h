#ifndef KEXEC_SH_H
#define KEXEC_SH_H

#define COMMAND_LINE_SIZE 2048

int uImage_sh_probe(const char *buf, off_t len);
int uImage_sh_load(int argc, char **argv, const char *buf, off_t len,
		        struct kexec_info *info);

int zImage_sh_probe(const char *buf, off_t len);
int zImage_sh_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void zImage_sh_usage(void);

int elf_sh_probe(const char *buf, off_t len);
int elf_sh_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void elf_sh_usage(void);

int netbsd_sh_probe(const char *buf, off_t len);
int netbsd_sh_load(int argc, char **argv, const char *buf, off_t len,
	struct kexec_info *info);
void netbsd_sh_usage(void);

char *get_append(void);
void kexec_sh_setup_zero_page(char *zero_page_buf, size_t zero_page_size,
			      char *cmd_line);

#endif /* KEXEC_SH_H */
