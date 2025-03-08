#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t strnlen(const char *s, size_t max);
void* memset(void* s, int c, size_t n);
void* memcpy(void *dest, const void *src, size_t len);
int memcmp(void *src1, void *src2, size_t len);


#endif /* STRING_H */
