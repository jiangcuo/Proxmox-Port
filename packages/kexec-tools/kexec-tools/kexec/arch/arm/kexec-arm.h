#ifndef KEXEC_ARM_H
#define KEXEC_ARM_H

#include <sys/types.h>

#define SYSFS_FDT "/sys/firmware/fdt"
#define BOOT_BLOCK_VERSION 17
#define BOOT_BLOCK_LAST_COMP_VERSION 16

extern off_t initrd_base, initrd_size;

int zImage_arm_probe(const char *buf, off_t len);
int zImage_arm_load(int argc, char **argv, const char *buf, off_t len,
		        struct kexec_info *info);
void zImage_arm_usage(void);

int uImage_arm_probe(const char *buf, off_t len);
int uImage_arm_load(int argc, char **argv, const char *buf, off_t len,
		        struct kexec_info *info);
extern int have_sysfs_fdt(void);

#endif /* KEXEC_ARM_H */
