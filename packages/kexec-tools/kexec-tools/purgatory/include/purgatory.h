#ifndef PURGATORY_H
#define PURGATORY_H

void putchar(int ch);
void sprintf(char *buffer, const char *fmt, ...)
	__attribute__ ((format (printf, 2, 3)));
void printf(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void setup_arch(void);
void post_verification_setup_arch(void);

#endif /* PURGATORY_H */
