/*
 * LoongArch binary image header.
 */

#if !defined(__LOONGARCH_IMAGE_HEADER_H)
#define __LOONGARCH_IMAGE_HEADER_H

#include <endian.h>
#include <stdint.h>

/**
 * struct loongarch_image_header
 *
 * @pe_sig: Optional PE format 'MZ' signature.
 * @reserved_1: Reserved.
 * @kernel_entry: Kernel image entry pointer.
 * @image_size: An estimated size of the memory image size in LSB byte order.
 * @text_offset: The image load offset in LSB byte order.
 * @reserved_2: Reserved.
 * @reserved_3: Reserved.
 * @pe_header: Optional offset to a PE format header.
 **/

struct loongarch_image_header {
	uint8_t pe_sig[2];
	uint16_t reserved_1[3];
	uint64_t kernel_entry;
	uint64_t image_size;
	uint64_t text_offset;
	uint64_t reserved_2[3];
	uint32_t reserved_3;
	uint32_t pe_header;
};

static const uint8_t loongarch_image_pe_sig[2] = {'M', 'Z'};
static const uint8_t loongarch_pe_machtype[6] = {'P','E', 0x0, 0x0, 0x64, 0x62};

/**
 * loongarch_header_check_pe_sig - Helper to check the loongarch image header.
 *
 * Returns non-zero if 'MZ' signature is found.
 */

static inline int loongarch_header_check_pe_sig(const struct loongarch_image_header *h)
{
	if (!h)
		return 0;

	return (h->pe_sig[0] == loongarch_image_pe_sig[0]
		&& h->pe_sig[1] == loongarch_image_pe_sig[1]);
}

static inline uint64_t loongarch_header_text_offset(
	const struct loongarch_image_header *h)
{
	if (!h)
		return 0;

	return le64toh(h->text_offset);
}

static inline uint64_t loongarch_header_image_size(
	const struct loongarch_image_header *h)
{
	if (!h)
		return 0;

	return le64toh(h->image_size);
}

static inline uint64_t loongarch_header_kernel_entry(
	const struct loongarch_image_header *h)
{
	if (!h)
		return 0;

	return le64toh(h->kernel_entry);
}

#endif
