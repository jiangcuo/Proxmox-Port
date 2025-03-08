/*
 * Global definition of all the bootwrapper operations.
 *
 * Author: Mark A. Greer <mgreer@mvista.com>
 *
 * 2006 (c) MontaVista Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#ifndef _PPC_BOOT_OPS_H_
#define _PPC_BOOT_OPS_H_
#include "types.h"

#define	MAX_PATH_LEN		256
#define	MAX_PROP_LEN		256 /* What should this be? */

typedef void (*kernel_entry_t)(unsigned long r3, unsigned long r4, void *r5);

/* Device Tree operations */
struct dt_ops {
	void *	(*finddevice)(const char *name);
	int	(*getprop)(const void *phandle, const char *name, void *buf,
			const int buflen);
	int	(*setprop)(const void *phandle, const char *name,
			const void *buf, const int buflen);
	void *(*get_parent)(const void *phandle);
	/* The node must not already exist. */
	void *(*create_node)(const void *parent, const char *name);
	void *(*find_node_by_prop_value)(const void *prev,
					 const char *propname,
					 const char *propval, int proplen);
	void *(*find_node_by_compatible)(const void *prev,
					 const char *compat);
	unsigned long (*finalize)(void);
	char *(*get_path)(const void *phandle, char *buf, int len);
};
extern struct dt_ops dt_ops;

void fdt_init(void *blob);
extern void flush_cache(void *, unsigned long);
int dt_xlate_reg(void *node, int res, unsigned long *addr, unsigned long *size);
int dt_xlate_addr(void *node, u32 *buf, int buflen, unsigned long *xlated_addr);
int dt_is_compatible(void *node, const char *compat);
void dt_get_reg_format(void *node, u32 *naddr, u32 *nsize);
int dt_get_virtual_reg(void *node, void **addr, int nres);

static inline void *finddevice(const char *name)
{
	return (dt_ops.finddevice) ? dt_ops.finddevice(name) : NULL;
}

static inline int getprop(void *devp, const char *name, void *buf, int buflen)
{
	return (dt_ops.getprop) ? dt_ops.getprop(devp, name, buf, buflen) : -1;
}

static inline int setprop(void *devp, const char *name,
			  const void *buf, int buflen)
{
	return (dt_ops.setprop) ? dt_ops.setprop(devp, name, buf, buflen) : -1;
}
#define setprop_val(devp, name, val) \
	do { \
		typeof(val) x = (val); \
		setprop((devp), (name), &x, sizeof(x)); \
	} while (0)

static inline int setprop_str(void *devp, const char *name, const char *buf)
{
	if (dt_ops.setprop)
		return dt_ops.setprop(devp, name, buf, strlen(buf) + 1);

	return -1;
}

static inline void *get_parent(const char *devp)
{
	return dt_ops.get_parent ? dt_ops.get_parent(devp) : NULL;
}

static inline void *create_node(const void *parent, const char *name)
{
	return dt_ops.create_node ? dt_ops.create_node(parent, name) : NULL;
}


static inline void *find_node_by_prop_value(const void *prev,
					    const char *propname,
					    const char *propval, int proplen)
{
	if (dt_ops.find_node_by_prop_value)
		return dt_ops.find_node_by_prop_value(prev, propname,
						      propval, proplen);

	return NULL;
}

static inline void *find_node_by_prop_value_str(const void *prev,
						const char *propname,
						const char *propval)
{
	return find_node_by_prop_value(prev, propname, propval,
				       strlen(propval) + 1);
}

static inline void *find_node_by_devtype(const void *prev,
					 const char *type)
{
	return find_node_by_prop_value_str(prev, "device_type", type);
}

static inline void *find_node_by_alias(const char *alias)
{
	void *devp = finddevice("/aliases");

	if (devp) {
		char path[MAX_PATH_LEN];
		if (getprop(devp, alias, path, MAX_PATH_LEN) > 0)
			return finddevice(path);
	}

	return NULL;
}

static inline void *find_node_by_compatible(const void *prev,
					    const char *compat)
{
	if (dt_ops.find_node_by_compatible)
		return dt_ops.find_node_by_compatible(prev, compat);

	return NULL;
}

#define dt_fixup_mac_addresses(...) \
	__dt_fixup_mac_addresses(0, __VA_ARGS__, NULL)


static inline char *get_path(const void *phandle, char *buf, int len)
{
	if (dt_ops.get_path)
		return dt_ops.get_path(phandle, buf, len);

	return NULL;
}

#endif /* _PPC_BOOT_OPS_H_ */
