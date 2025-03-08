#ifndef CRASHDUMP_SH_H
#define CRASHDUMP_SH_H

struct kexec_info;
int load_crashdump_segments(struct kexec_info *info, char* mod_cmdline);

#define PAGE_OFFSET	0x80000000

#endif /* CRASHDUMP_SH_H */
