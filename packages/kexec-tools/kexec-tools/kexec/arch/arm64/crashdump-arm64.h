/*
 * ARM64 crashdump.
 *
 * Copyright (c) 2014-2017 Linaro Limited
 * Author: AKASHI Takahiro <takahiro.akashi@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CRASHDUMP_ARM64_H
#define CRASHDUMP_ARM64_H

#include "kexec.h"

#define CRASH_MAX_MEMORY_RANGES	32768

/* crash dump kernel support at most two regions, low_region and high region. */
#define CRASH_MAX_RESERVED_RANGES	2

extern struct memory_ranges usablemem_rgns;
extern struct memory_range crash_reserved_mem[];
extern struct memory_range elfcorehdr_mem;

extern int load_crashdump_segments(struct kexec_info *info);
extern void fixup_elf_addrs(struct mem_ehdr *ehdr);

#endif /* CRASHDUMP_ARM64_H */
