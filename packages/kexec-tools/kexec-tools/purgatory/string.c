#include <stddef.h>
#include <string.h>

size_t strnlen(const char *s, size_t max)
{
	size_t len = 0;
	while(len < max && *s) {
		len++;
		s++;
	}
	return len;
}

void* memset(void* s, int c, size_t n)
{
	size_t i;
	char *ss = (char*)s;

	for (i=0;i<n;i++) ss[i] = c;
	return s;
}


void* memcpy(void *dest, const void *src, size_t len)
{
	size_t i;
	unsigned char *d;
	const unsigned char *s;
	d = dest;
	s = src;

	for (i=0; i < len; i++) 
		d[i] = s[i];

	return dest;
}


int memcmp(void *src1, void *src2, size_t len)
{
	unsigned char *s1, *s2;
	size_t i;
	s1 = src1;
	s2 = src2;
	for(i = 0; i < len; i++) {
		if (*s1 != *s2) {
			return *s2 - *s1;
		}
		s1++;
		s2++;
	}
	return 0;
	
}

