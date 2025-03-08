#ifndef __KEXEC_LZMA_H
#define __KEXEC_LZMA_H

#include <sys/types.h>

char *lzma_decompress_file(const char *filename, off_t *r_size);

#endif /* __KEXEC_LZMA_H */
