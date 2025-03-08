#ifndef ARCH_I386_IO_H
#define ARCH_I386_IO_H

#include <stdint.h>
/* Helper functions for directly doing I/O */

static inline __attribute__((__always_inline__))
uint8_t inb(uint16_t port)
{
	uint8_t result;

	__asm__ __volatile__ (
		"inb %w1,%0"
		:"=a" (result)
		:"Nd" (port));
	return result;
}

static inline __attribute__((__always_inline__))
uint16_t inw(uint16_t port)
{
	uint16_t result;

	__asm__ __volatile__ (
		"inw %w1,%0"
		:"=a" (result)
		:"Nd" (port));
	return result;
}

static inline __attribute__((__always_inline__))
uint32_t inl(uint32_t port)
{
	uint32_t result;

	__asm__ __volatile__ (
		"inl %w1,%0"
		:"=a" (result)
		:"Nd" (port));
	return result;
}

static inline __attribute__((__always_inline__))
void outb (uint8_t value, uint16_t port)
{
	__asm__ __volatile__ (
		"outb %b0,%w1"
		:
		:"a" (value), "Nd" (port));
}

static inline __attribute__((__always_inline__))
void outw (uint16_t value, uint16_t port)
{
	__asm__ __volatile__ (
		"outw %w0,%w1"
		:
		:"a" (value), "Nd" (port));
}

static inline __attribute__((__always_inline__))
void outl (uint32_t value, uint16_t port)
{
	__asm__ __volatile__ (
		"outl %0,%w1"
		:
		:"a" (value), "Nd" (port));
}


/*
 * readX/writeX() are used to access memory mapped devices. On some
 * architectures the memory mapped IO stuff needs to be accessed
 * differently. On the x86 architecture, we just read/write the
 * memory location directly.
 */

static inline __attribute__((__always_inline__))
unsigned char readb(const volatile void  *addr)
{
	return *(volatile unsigned char *) addr;
}
static inline __attribute__((__always_inline__))
unsigned short readw(const volatile void  *addr)
{
	return *(volatile unsigned short *) addr;
}
static inline __attribute__((__always_inline__))
unsigned int readl(const volatile void  *addr)
{
	return *(volatile unsigned int *) addr;
}

static inline __attribute__((__always_inline__))
void writeb(unsigned char b, volatile void  *addr)
{
	*(volatile unsigned char *) addr = b;
}
static inline __attribute__((__always_inline__))
void writew(unsigned short b, volatile void  *addr)
{
	*(volatile unsigned short *) addr = b;
}
static inline __attribute__((__always_inline__))
void writel(unsigned int b, volatile void  *addr)
{
	*(volatile unsigned int *) addr = b;
}

#endif /* ARCH_I386_IO_H */
