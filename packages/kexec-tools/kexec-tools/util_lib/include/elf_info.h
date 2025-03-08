#ifndef ELF_INFO_H
#define ELF_INFO_H

#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#define _LARGEFILE_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include <endian.h>
#include <byteswap.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <elf.h>
#include <stdbool.h>
#include <inttypes.h>
#include <ctype.h>

int get_pt_load(int idx,
	unsigned long long *phys_start,
	unsigned long long *phys_end,
	unsigned long long *virt_start,
	unsigned long long *virt_end);
int read_phys_offset_elf_kcore(int fd, long *phys_off);
int read_elf(int fd);
void dump_dmesg(int fd, void (*handler)(char*, unsigned int));
extern void (*arch_scan_vmcoreinfo)(char *pos);

#endif /* ELF_INFO_H */
