
#include <limits.h>
#include <stdint.h>
#include <purgatory.h>
#include <sha256.h>
#include <string.h>
#include "../kexec/kexec-sha256.h"

struct sha256_region sha256_regions[SHA256_REGIONS] = {};
sha256_digest_t sha256_digest = { };
int skip_checks = 0;

int verify_sha256_digest(void)
{
	struct sha256_region *ptr, *end;
	sha256_digest_t digest;
	size_t i;
	sha256_context ctx;
	sha256_starts(&ctx);
	end = &sha256_regions[sizeof(sha256_regions)/sizeof(sha256_regions[0])];
	for(ptr = sha256_regions; ptr < end; ptr++) {
		sha256_update(&ctx, (uint8_t *)((uintptr_t)ptr->start),
			      ptr->len);
	}
	sha256_finish(&ctx, digest);
	if (memcmp(digest, sha256_digest, sizeof(digest)) != 0) {
		printf("sha256 digests do not match :(\n");
		printf("       digest: ");
		for(i = 0; i < sizeof(digest); i++) {
			printf("%hhx ", digest[i]);
		}
		printf("\n");
		printf("sha256_digest: ");
		for(i = 0; i < sizeof(sha256_digest); i++) {
			printf("%hhx ", sha256_digest[i]);
		}
		printf("\n");
		return 1;
	}
	return 0;
}

void purgatory(void)
{
	printf("I'm in purgatory\n");
	setup_arch();
	if (!skip_checks && verify_sha256_digest()) {
		for(;;) {
			/* loop forever */
		}
	}
	post_verification_setup_arch();
}
