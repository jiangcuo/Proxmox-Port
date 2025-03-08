#if !defined(KEXEC_DT_OPS_H)
#define KEXEC_DT_OPS_H

#include <sys/types.h>

int dtb_set_initrd(char **dtb, off_t *dtb_size, off_t start, off_t end);
void dtb_clear_initrd(char **dtb, off_t *dtb_size);
int dtb_set_bootargs(char **dtb, off_t *dtb_size, const char *command_line);
int dtb_set_property(char **dtb, off_t *dtb_size, const char *node,
	const char *prop, const void *value, int value_len);

int dtb_delete_property(char *dtb, const char *node, const char *prop);

#endif
