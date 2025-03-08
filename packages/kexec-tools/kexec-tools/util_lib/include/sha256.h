#ifndef SHA256_H
#define SHA256_H

#include <sys/types.h>
#include <stdint.h>

typedef struct
{
	size_t total[2];
	uint32_t state[8];
	uint8_t  buffer[64];
}
sha256_context;

typedef uint8_t sha256_digest_t[32];

void sha256_starts( sha256_context *ctx );
void sha256_update( sha256_context *ctx, const uint8_t *input, size_t length );
void sha256_finish( sha256_context *ctx, sha256_digest_t digest );


#endif /* SHA256_H */
