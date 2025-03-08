#ifndef __FIXUP_DTB_H
#define __FIXUP_DTB_H

char *fixup_dtb_init(struct kexec_info *info, char *blob_buf, off_t *blob_size,
			unsigned long hole_addr, unsigned long *dtb_addr);

char *fixup_dtb_finalize(struct kexec_info *info, char *blob_buf, off_t *blob_size,
			char *nodes[], char *cmdline);

#endif
