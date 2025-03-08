#ifndef __KEXEC_PE_ZBOOT_H__
#define __KEXEC_PE_ZBOOT_H__

/* see drivers/firmware/efi/libstub/zboot-header.S */
struct linux_pe_zboot_header {
	uint32_t mz_magic;
	uint32_t image_type;
	uint32_t payload_offset;
	uint32_t payload_size;
	uint32_t reserved[2];
	uint32_t compress_type;
};

int pez_prepare(const char *crude_buf, off_t buf_sz, int *kernel_fd,
		off_t *kernel_size);
#endif
