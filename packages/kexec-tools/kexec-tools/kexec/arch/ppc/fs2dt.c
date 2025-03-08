/*
 * fs2dt: creates a flattened device-tree
 *
 * Copyright (C) 2004,2005  Milton D Miller II, IBM Corporation
 * Copyright (C) 2005  R Sharada (sharada@in.ibm.com), IBM Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation (version 2 of the License).
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "../../kexec.h"
#include "kexec-ppc.h"
#include "types.h"

#define MAXPATH			1024	/* max path name length */
#define NAMESPACE		16384	/* max bytes for property names */
#define TREEWORDS		65536	/* max 32 bit words for properties */
#define MEMRESERVE		256	/* max number of reserved memory blks */
#define MAX_MEMORY_RANGES	1024

static char pathname[MAXPATH];
static char propnames[NAMESPACE] = { 0 };
static unsigned dtstruct[TREEWORDS], *dt;
static unsigned long long mem_rsrv[2*MEMRESERVE] = { 0, 0 };

static int crash_param;
static char local_cmdline[COMMAND_LINE_SIZE] = { "" };
static unsigned *dt_len; /* changed len of modified cmdline
			    in flat device-tree */
static struct bootblock bb[1];

void reserve(unsigned long long where, unsigned long long length)
{
	size_t offset;

	for (offset = 0; mem_rsrv[offset + 1]; offset += 2)
		;

	if (offset + 4 >= 2 * MEMRESERVE)
		die("unrecoverable error: exhasuted reservation meta data\n");

	mem_rsrv[offset] = where;
	mem_rsrv[offset + 1] = length;
	mem_rsrv[offset + 3] = 0;  /* N.B: don't care about offset + 2 */
}

/* look for properties we need to reserve memory space for */
static void checkprop(char *name, unsigned *data, int len)
{
	static unsigned long long base, size, end;

	if ((data == NULL) && (base || size || end))
		die("unrecoverable error: no property data");
	else if (!strcmp(name, "linux,rtas-base"))
		base = *data;
	else if (!strcmp(name, "linux,tce-base"))
		base = *(unsigned long long *) data;
	else if (!strcmp(name, "rtas-size") ||
			!strcmp(name, "linux,tce-size"))
		size = *data;
	else if (reuse_initrd && !strcmp(name, "linux,initrd-start"))
		if (len == 8)
			base = *(unsigned long long *) data;
		else
			base = *data;
	else if (reuse_initrd && !strcmp(name, "linux,initrd-end"))
		end = *(unsigned long long *) data;

	if (size && end)
		die("unrecoverable error: size and end set at same time\n");
	if (base && size) {
		reserve(base, size);
		base = 0;
		size = 0;
	}
	if (base && end) {
		reserve(base, end-base);
		base = 0;
		end = 0;
	}
}

/*
 * return the property index for a property name, creating a new one
 * if needed.
 */
static unsigned propnum(const char *name)
{
	unsigned offset = 0;

	while (propnames[offset])
		if (strcmp(name, propnames+offset))
			offset += strlen(propnames+offset)+1;
		else
			return offset;

	if (NAMESPACE - offset < strlen(name) + 1)
		die("unrecoverable error: propnames overrun\n");

	strcpy(propnames+offset, name);

	return offset;
}

static void add_usable_mem_property(int fd, int len)
{
	char fname[MAXPATH], *bname;
	unsigned long buf[2];
	unsigned long ranges[2*MAX_MEMORY_RANGES];
	unsigned long long base, end, loc_base, loc_end;
	int range, rlen = 0;

	strcpy(fname, pathname);
	bname = strrchr(fname, '/');
	bname[0] = '\0';
	bname = strrchr(fname, '/');
	if (strncmp(bname, "/memory@", 8) && strcmp(bname, "/memory"))
		return;

	if (lseek(fd, 0, SEEK_SET) < 0)
		die("unrecoverable error: error seeking in \"%s\": %s\n",
		    pathname, strerror(errno));
	if (read_memory_region_limits(fd, &base, &end) != 0)
		die("unrecoverable error: error parsing memory/reg limits\n");

	for (range = 0; range < usablemem_rgns.size; range++) {
		loc_base = usablemem_rgns.ranges[range].start;
		loc_end = usablemem_rgns.ranges[range].end;
		if (loc_base >= base && loc_end <= end) {
			ranges[rlen++] = loc_base;
			ranges[rlen++] = loc_end - loc_base;
		} else if (base < loc_end && end > loc_base) {
			if (loc_base < base)
				loc_base = base;
			if (loc_end > end)
				loc_end = end;
			ranges[rlen++] = loc_base;
			ranges[rlen++] = loc_end - loc_base;
		}
	}

	if (!rlen) {
		/*
		 * User did not pass any ranges for thsi region. Hence, write
		 * (0,0) duple in linux,usable-memory property such that
		 * this region will be ignored.
		 */
		ranges[rlen++] = 0;
		ranges[rlen++] = 0;
	}

	rlen = rlen * sizeof(unsigned long);
	/*
	 * No add linux,usable-memory property.
	 */
	*dt++ = 3;
	*dt++ = rlen;
	*dt++ = propnum("linux,usable-memory");
	memcpy(dt, &ranges, rlen);
	dt += (rlen + 3)/4;
}

/* put all properties (files) in the property structure */
static void putprops(char *fn, struct dirent **nlist, int numlist)
{
	struct dirent *dp;
	int i = 0, fd, len;
	struct stat statbuf;

	for (i = 0; i < numlist; i++) {
		dp = nlist[i];
		strcpy(fn, dp->d_name);

		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;

		if (lstat(pathname, &statbuf))
			die("unrecoverable error: could not stat \"%s\": %s\n",
			    pathname, strerror(errno));

		if (!crash_param && !strcmp(fn, "linux,crashkernel-base"))
			continue;

		if (!crash_param && !strcmp(fn, "linux,crashkernel-size"))
			continue;

		/*
		 * This property will be created for each node during kexec
		 * boot. So, ignore it.
		 */
		if (!strcmp(dp->d_name, "linux,pci-domain") ||
			!strcmp(dp->d_name, "linux,htab-base") ||
			!strcmp(dp->d_name, "linux,htab-size") ||
			!strcmp(dp->d_name, "linux,kernel-end") ||
			!strcmp(dp->d_name, "linux,usable-memory"))
				continue;

		/* This property will be created/modified later in putnode()
		 * So ignore it, unless we are reusing the initrd.
		 */
		if ((!strcmp(dp->d_name, "linux,initrd-start") ||
		     !strcmp(dp->d_name, "linux,initrd-end")) &&
		    !reuse_initrd)
				continue;

		if (!S_ISREG(statbuf.st_mode))
			continue;

		len = statbuf.st_size;

		*dt++ = 3;
		dt_len = dt;
		*dt++ = len;
		*dt++ = propnum(fn);

		fd = open(pathname, O_RDONLY);
		if (fd == -1)
			die("unrecoverable error: could not open \"%s\": %s\n",
			    pathname, strerror(errno));

		if (read(fd, dt, len) != len)
			die("unrecoverable error: could not read \"%s\": %s\n",
			    pathname, strerror(errno));

		checkprop(fn, dt, len);

		/* Get the cmdline from the device-tree and modify it */
		if (!strcmp(dp->d_name, "bootargs")) {
			int cmd_len;
			char temp_cmdline[COMMAND_LINE_SIZE] = { "" };
			char *param = NULL;
			cmd_len = strlen(local_cmdline);
			if (cmd_len != 0) {
				param = strstr(local_cmdline, "crashkernel=");
				if (param)
					crash_param = 1;
				param = strstr(local_cmdline, "root=");
			}
			if (!param) {
				char *old_param;
				memcpy(temp_cmdline, dt, len);
				param = strstr(temp_cmdline, "root=");
				if (param) {
					old_param = strtok(param, " ");
					if (cmd_len != 0)
						strcat(local_cmdline, " ");
					strcat(local_cmdline, old_param);
				}
			}
			strcat(local_cmdline, " ");
			cmd_len = strlen(local_cmdline);
			cmd_len = cmd_len + 1;
			memcpy(dt, local_cmdline, cmd_len);
			len = cmd_len;
			*dt_len = cmd_len;

			dbgprintf("Modified cmdline:%s\n", local_cmdline);

		}

		dt += (len + 3)/4;
		if (!strcmp(dp->d_name, "reg") && usablemem_rgns.size)
			add_usable_mem_property(fd, len);
		close(fd);
	}

	fn[0] = '\0';
	checkprop(pathname, NULL, 0);
}

/*
 * Compare function used to sort the device-tree directories
 * This function will be passed to scandir.
 */
static int comparefunc(const void *dentry1, const void *dentry2)
{
	char *str1 = (*(struct dirent **)dentry1)->d_name;
	char *str2 = (*(struct dirent **)dentry2)->d_name;
	char *p1, *p2;
	int res = 0, max_len;

	/*
	 * strcmp scans from left to right and fails to idetify for some
	 * strings such as memory@10000000 and memory@f000000.
	 * Therefore, we get the wrong sorted order like memory@10000000 and
	 * memory@f000000.
	 */
	if ((p1 = strchr(str1, '@')) && (p2 = strchr(str2, '@'))) {
		max_len = max(p1 - str1, p2 - str2);
		if ((res = strncmp(str1, str2, max_len)) == 0) {
			/* prefix is equal - compare part after '@' by length */
			p1++; p2++;
			res = strlen(p1) - strlen(p2);
			if (res == 0)
				/* equal length, compare by strcmp() */
				res = strcmp(p1,p2);
		}
        } else {
		res = strcmp(str1, str2);
        }

	return res;
}

/*
 * put a node (directory) in the property structure.  first properties
 * then children.
 */
static void putnode(void)
{
	char *dn;
	struct dirent *dp;
	char *basename;
	struct dirent **namelist;
	int numlist, i;
	struct stat statbuf;

	numlist = scandir(pathname, &namelist, 0, comparefunc);
	if (numlist < 0)
		die("unrecoverable error: could not scan \"%s\": %s\n",
		    pathname, strerror(errno));
	if (numlist == 0)
		die("unrecoverable error: no directory entries in \"%s\"",
		    pathname);

	basename = strrchr(pathname, '/') + 1;

	*dt++ = 1;
	strcpy((void *)dt, *basename ? basename : "");
	dt += strlen((void *)dt) / sizeof(unsigned) + 1;

	strcat(pathname, "/");
	dn = pathname + strlen(pathname);

	putprops(dn, namelist, numlist);

	/* 
	 * Add initrd entries to the second kernel
	 * if
	 * 	a) a ramdisk is specified in cmdline
	 * 	 OR
	 * 	b) reuseinitrd is specified and a initrd is
	 * 	   used by the kernel.
	 *
	 */
	if ((ramdisk || (initrd_base && reuse_initrd))
		&& !strcmp(basename, "chosen/")) {
		int len = 8;
		unsigned long long initrd_end;
		*dt++ = 3;
		*dt++ = len;
		*dt++ = propnum("linux,initrd-start");

		memcpy(dt, &initrd_base, len);
		dt += (len + 3)/4;

		len = 8;
		*dt++ = 3;
		*dt++ = len;
		*dt++ = propnum("linux,initrd-end");

		initrd_end = initrd_base + initrd_size;

		memcpy(dt, &initrd_end, len);
		dt += (len + 3)/4;
		/* reserve the existing initrd image in case of reuse_initrd */
		if (initrd_base && initrd_size && reuse_initrd)
			reserve(initrd_base, initrd_size);
	}

	for (i = 0; i < numlist; i++) {
		dp = namelist[i];
		strcpy(dn, dp->d_name);
		free(namelist[i]);

		if (!strcmp(dn, ".") || !strcmp(dn, ".."))
			continue;

		if (lstat(pathname, &statbuf))
			die("unrecoverable error: could not stat \"%s\": %s\n",
			    pathname, strerror(errno));

		if (S_ISDIR(statbuf.st_mode))
			putnode();
	}

	*dt++ = 2;
	dn[-1] = '\0';
	free(namelist);
}

int create_flatten_tree(struct kexec_info *info, unsigned char **bufp,
			unsigned long *sizep, char *cmdline)
{
	unsigned long len;
	unsigned long tlen;
	unsigned char *buf;
	unsigned long me;

	me = 0;

	strcpy(pathname, "/proc/device-tree/");

	dt = dtstruct;

	if (cmdline)
		strcpy(local_cmdline, cmdline);

	putnode();
	*dt++ = 9;

	len = _ALIGN(sizeof(bb[0]), 8);

	bb->off_mem_rsvmap = len;

	for (len = 1; mem_rsrv[len]; len += 2)
		;
	len += 3;
	len *= sizeof(mem_rsrv[0]);

	bb->off_dt_struct = bb->off_mem_rsvmap + len;

	len = dt - dtstruct;
	len *= sizeof(unsigned);
	bb->dt_struct_size = len;
	bb->off_dt_strings = bb->off_dt_struct + len;

	len = propnum("");
	bb->dt_strings_size = len;
	len = _ALIGN(len, 4);
	bb->totalsize = bb->off_dt_strings + len;

	bb->magic = 0xd00dfeed;
	bb->version = 17;
	bb->last_comp_version = 16;

	reserve(me, bb->totalsize); /* patched later in kexec_load */

	buf = (unsigned char *) malloc(bb->totalsize);
	*bufp = buf;
	memcpy(buf, bb, bb->off_mem_rsvmap);
	tlen = bb->off_mem_rsvmap;
	memcpy(buf+tlen, mem_rsrv, bb->off_dt_struct - bb->off_mem_rsvmap);
	tlen = tlen + (bb->off_dt_struct - bb->off_mem_rsvmap);
	memcpy(buf+tlen, dtstruct,  bb->off_dt_strings - bb->off_dt_struct);
	tlen = tlen +  (bb->off_dt_strings - bb->off_dt_struct);
	memcpy(buf+tlen, propnames,  bb->totalsize - bb->off_dt_strings);
	tlen = tlen + bb->totalsize - bb->off_dt_strings;
	*sizep = tlen;
	return 0;
}
