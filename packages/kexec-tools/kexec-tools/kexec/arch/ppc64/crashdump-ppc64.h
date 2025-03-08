#ifndef CRASHDUMP_PPC64_H
#define CRASHDUMP_PPC64_H

#include <stdint.h>
#include <sys/types.h>

struct kexec_info;
int load_crashdump_segments(struct kexec_info *info, char *mod_cmdline,
				uint64_t max_addr, unsigned long min_base);
void add_usable_mem_rgns(unsigned long long base, unsigned long long size);

#define PAGE_OFFSET     0xC000000000000000ULL
#define KERNELBASE      PAGE_OFFSET
#define VMALLOCBASE     0xD000000000000000ULL

#define __pa(x)         ((unsigned long)(x)-PAGE_OFFSET)
#define MAXMEM          (-(unsigned long)(KERNELBASE-VMALLOCBASE))

#define COMMAND_LINE_SIZE       2048 /* from kernel */
/* Backup Region, First 64K of System RAM. */
#define BACKUP_SRC_START    0x0000
#define BACKUP_SRC_END      0xffff
#define BACKUP_SRC_SIZE     (BACKUP_SRC_END - BACKUP_SRC_START + 1)

#define KDUMP_BACKUP_LIMIT	BACKUP_SRC_SIZE

#define KERNEL_RUN_AT_ZERO_MAGIC 0x72756e30	/* "run0" */

extern uint64_t crash_base;
extern uint64_t crash_size;
extern uint64_t memory_limit;
extern unsigned int rtas_base;
extern unsigned int rtas_size;
extern uint64_t opal_base;
extern uint64_t opal_size;

/*
 * In case of ibm,dynamic-memory-v2 property, this is the number of LMB
 * sets where each set represents a group of sequential LMB entries. In
 * case of ibm,dynamic-memory property, the number of LMB sets is nothing
 * but the total number of LMB entries.
 */
extern unsigned int num_of_lmb_sets;
extern unsigned int is_dyn_mem_v2;
extern uint64_t lmb_size;

#define LMB_ENTRY_SIZE	24
#define DRCONF_ADDR	(is_dyn_mem_v2 ? 4 : 0)
#define DRCONF_FLAGS	20

#endif /* CRASHDUMP_PPC64_H */
