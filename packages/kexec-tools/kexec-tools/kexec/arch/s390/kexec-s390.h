/*
 * kexec/arch/s390/kexec-s390.h
 *
 * (C) Copyright IBM Corp. 2005
 *
 * Author(s): Rolf Adelsberger <adelsberger@de.ibm.com>
 *
 */

#ifndef KEXEC_S390_H
#define KEXEC_S390_H

#define IMAGE_READ_OFFSET           0x10000

#define RAMDISK_ORIGIN_ADDR         0x800000
#define INITRD_START_OFFS           0x408
#define INITRD_SIZE_OFFS            0x410
#define OLDMEM_BASE_OFFS            0x418
#define OLDMEM_SIZE_OFFS            0x420
#define MAX_COMMAND_LINESIZE_OFFS   0x430
#define COMMAND_LINE_OFFS           0x480
#define LEGACY_COMMAND_LINESIZE     896
#define MAX_MEMORY_RANGES           1024

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

extern int image_s390_load(int, char **, const char *, off_t, struct kexec_info *);
extern int image_s390_probe(const char *, off_t);
extern void image_s390_usage(void);
extern int load_crashdump_segments(struct kexec_info *info,
				   unsigned long crash_base,
				   unsigned long crash_end);
extern int get_memory_ranges_s390(struct memory_range range[], int *ranges,
				  int with_crashk);
extern int command_line_add(struct kexec_info *info, const char *str);

#endif /* KEXEC_S390_H */
