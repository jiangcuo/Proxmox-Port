#ifndef __KEXEC_ZLIB_H
#define __KEXEC_ZLIB_H

#include <stdio.h>
#include <sys/types.h>

#include "config.h"

int is_zlib_file(const char *filename, off_t *r_size);
char *zlib_decompress_file(const char *filename, off_t *r_size);
#endif /* __KEXEC_ZLIB_H */
