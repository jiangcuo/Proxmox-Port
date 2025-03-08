/*
 * ARM64 binary image header.
 */

#if !defined(__ARM64_IMAGE_HEADER_H)
#define __ARM64_IMAGE_HEADER_H

#include <endian.h>
#include <stdint.h>

/**
 * struct arm64_image_header - arm64 kernel image header.
 *
 * @pe_sig: Optional PE format 'MZ' signature.
 * @branch_code: Reserved for instructions to branch to stext.
 * @text_offset: The image load offset in LSB byte order.
 * @image_size: An estimated size of the memory image size in LSB byte order.
 * @flags: Bit flags in LSB byte order:
 *   Bit 0:   Image byte order: 1=MSB.
 *   Bit 1-2: Kernel page size: 1=4K, 2=16K, 3=64K.
 *   Bit 3:   Image placement: 0=low.
 * @reserved_1: Reserved.
 * @magic: Magic number, "ARM\x64".
 * @pe_header: Optional offset to a PE format header.
 **/

struct arm64_image_header {
	uint8_t pe_sig[2];
	uint16_t branch_code[3];
	uint64_t text_offset;
	uint64_t image_size;
	uint64_t flags;
	uint64_t reserved_1[3];
	uint8_t magic[4];
	uint32_t pe_header;
};

static const uint8_t arm64_image_magic[4] = {'A', 'R', 'M', 0x64U};
static const uint8_t arm64_image_pe_sig[2] = {'M', 'Z'};
static const uint8_t arm64_pe_machtype[6] = {'P','E', 0x0, 0x0, 0x64, 0xAA};
static const uint64_t arm64_image_flag_be = (1UL << 0);
static const uint64_t arm64_image_flag_page_size = (3UL << 1);
static const uint64_t arm64_image_flag_placement = (1UL << 3);

/**
 * enum arm64_header_page_size
 */

enum arm64_header_page_size {
	arm64_header_page_size_invalid = 0,
	arm64_header_page_size_4k,
	arm64_header_page_size_16k,
	arm64_header_page_size_64k
};

/**
 * arm64_header_check_magic - Helper to check the arm64 image header.
 *
 * Returns non-zero if header is OK.
 */

static inline int arm64_header_check_magic(const struct arm64_image_header *h)
{
	if (!h)
		return 0;

	return (h->magic[0] == arm64_image_magic[0]
		&& h->magic[1] == arm64_image_magic[1]
		&& h->magic[2] == arm64_image_magic[2]
		&& h->magic[3] == arm64_image_magic[3]);
}

/**
 * arm64_header_check_pe_sig - Helper to check the arm64 image header.
 *
 * Returns non-zero if 'MZ' signature is found.
 */

static inline int arm64_header_check_pe_sig(const struct arm64_image_header *h)
{
	if (!h)
		return 0;

	return (h->pe_sig[0] == arm64_image_pe_sig[0]
		&& h->pe_sig[1] == arm64_image_pe_sig[1]);
}

/**
 * arm64_header_check_msb - Helper to check the arm64 image header.
 *
 * Returns non-zero if the image was built as big endian.
 */

static inline int arm64_header_check_msb(const struct arm64_image_header *h)
{
	if (!h)
		return 0;

	return (le64toh(h->flags) & arm64_image_flag_be) >> 0;
}

/**
 * arm64_header_page_size
 */

static inline enum arm64_header_page_size arm64_header_page_size(
	const struct arm64_image_header *h)
{
	if (!h)
		return 0;

	return (le64toh(h->flags) & arm64_image_flag_page_size) >> 1;
}

/**
 * arm64_header_placement
 *
 * Returns non-zero if the image has no physical placement restrictions.
 */

static inline int arm64_header_placement(const struct arm64_image_header *h)
{
	if (!h)
		return 0;

	return (le64toh(h->flags) & arm64_image_flag_placement) >> 3;
}

static inline uint64_t arm64_header_text_offset(
	const struct arm64_image_header *h)
{
	if (!h)
		return 0;

	return le64toh(h->text_offset);
}

static inline uint64_t arm64_header_image_size(
	const struct arm64_image_header *h)
{
	if (!h)
		return 0;

	return le64toh(h->image_size);
}

#endif
