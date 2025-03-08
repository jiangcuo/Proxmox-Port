#ifndef KEXEC_SHA256_H
#define KEXEC_SHA256_H

struct sha256_region {
	uint64_t start;
	uint64_t len;
};

#define SHA256_REGIONS 16

#endif /* KEXEC_SHA256_H */
