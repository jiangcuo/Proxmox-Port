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

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "kexec.h"
#include "fs2dt.h"
#include "libfdt/fdt.h"

#define MAXPATH 1024		/* max path name length */
#define NAMESPACE 16384		/* max bytes for property names */
#define INIT_TREE_WORDS 65536	/* Initial num words for prop values */
#define MEMRESERVE 256		/* max number of reserved memory blocks */
#define MEM_RANGE_CHUNK_SZ 2048 /* Initial num dwords for mem ranges */

static char pathname[MAXPATH];
static char propnames[NAMESPACE] = { 0 };
static unsigned *dt_base, *dt;
static unsigned int dt_cur_size;
static unsigned long long mem_rsrv[2*MEMRESERVE] = { 0ULL, 0ULL };

static int crash_param = 0;
static char local_cmdline[COMMAND_LINE_SIZE] = { "" };

extern unsigned char reuse_initrd;

/* Used for enabling printing message from purgatory code
 * Only has implemented for PPC64 */
int my_debug;
int dt_no_old_root;

/* This provides the behaviour of hte existing ppc64 implementation */
static void pad_structure_block(size_t len) {
#ifdef NEED_STRUCTURE_BLOCK_EXTRA_PAD
	if ((len >= 8) && ((unsigned long)dt & 0x4))
		dt++;
#endif
}

/* Before we add something to the dt, reserve N words using this.
 * If there isn't enough room, it's realloced -- and you don't overflow and
 * splat bits of your heap.
 */
static void dt_reserve(unsigned **dt_ptr, unsigned words)
{
	unsigned int sz = INIT_TREE_WORDS;

	if (sz < words)
		sz = words;

	if (((*dt_ptr - dt_base) + words) >= dt_cur_size) {
		int offset;
		unsigned int new_size = dt_cur_size + sz;
		unsigned *new_dt = realloc(dt_base, new_size*4);

		if (!new_dt)
			die("unrecoverable error: Can't realloc %d bytes for "
			    "device tree\n", new_size*4);
		offset = *dt_ptr - dt_base;
		dt_base = new_dt;
		dt_cur_size = new_size;
		*dt_ptr = dt_base + offset;
		memset(*dt_ptr, 0, (new_size - offset)*4);
	}
}

void reserve(unsigned long long where, unsigned long long length)
{
	size_t offset;

	for (offset = 0; be64_to_cpu(mem_rsrv[offset + 1]); offset += 2)
		;

	if (offset + 4 >= 2 * MEMRESERVE)
		die("unrecoverable error: exhasuted reservation meta data\n");

	mem_rsrv[offset] = cpu_to_be64(where);
	mem_rsrv[offset + 1] = cpu_to_be64(length);
	mem_rsrv[offset + 2] = mem_rsrv[offset + 3] = cpu_to_be64(0);
}

/* look for properties we need to reserve memory space for */
static void checkprop(char *name, unsigned *data, int len)
{
	static unsigned long long base, size, end;

	if ((data == NULL) && (base || size || end))
		die("unrecoverable error: no property data");
	else if (!strcmp(name, "linux,rtas-base"))
		base = be32_to_cpu(*data);
	else if (!strcmp(name, "opal-base-address"))
		base = be64_to_cpu(*(unsigned long long *)data);
	else if (!strcmp(name, "opal-runtime-size"))
		size = be64_to_cpu(*(unsigned long long *)data);
	else if (!strcmp(name, "linux,tce-base"))
		base = be64_to_cpu(*(unsigned long long *) data);
	else if (!strcmp(name, "rtas-size") ||
			!strcmp(name, "linux,tce-size"))
		size = be32_to_cpu(*data);
	else if (reuse_initrd && !strcmp(name, "linux,initrd-start")) {
		if (len == 8)
			base = be64_to_cpu(*(unsigned long long *) data);
		else
			base = be32_to_cpu(*data);
	} else if (reuse_initrd && !strcmp(name, "linux,initrd-end")) {
		if (len == 8)
			end = be64_to_cpu(*(unsigned long long *) data);
		else
			end = be32_to_cpu(*data);
	}

	if (size && end)
		die("unrecoverable error: size and end set at same time\n");
	if (base && size) {
		reserve(base, size);
		base = size = 0;
	}
	if (base && end) {
		reserve(base, end-base);
		base = end = 0;
	}
}

/*
 * return the property index for a property name, creating a new one
 * if needed.
 */
static unsigned propnum(const char *name)
{
	unsigned offset = 0;

	while(propnames[offset])
		if (strcmp(name, propnames+offset))
			offset += strlen(propnames+offset)+1;
		else
			return offset;

	if (NAMESPACE - offset < strlen(name) + 1)
		die("unrecoverable error: propnames overrun\n");

	strcpy(propnames+offset, name);

	return offset;
}

/*
 * Add ranges by comparing 'base' and 'end' addresses with usable
 * memory ranges. Returns the number of ranges added. Each range added
 * increments 'idx' by 2.
 */
static uint64_t add_ranges(uint64_t **ranges, int *ranges_size, int idx,
			   uint64_t base, uint64_t end)
{
	uint64_t loc_base, loc_end, rngs_cnt = 0;
	size_t range;
	int add = 0;

	for (range = 0; range < usablemem_rgns.size; range++) {
		loc_base = usablemem_rgns.ranges[range].start;
		loc_end = usablemem_rgns.ranges[range].end;
		if (loc_base >= base && loc_end <= end) {
			add = 1;
		} else if (base < loc_end && end > loc_base) {
			if (loc_base < base)
				loc_base = base;
			if (loc_end > end)
				loc_end = end;
			add = 1;
		}

		if (add) {
			if (idx >= ((*ranges_size) - 2)) {
				(*ranges_size) += MEM_RANGE_CHUNK_SZ;
				*ranges = realloc(*ranges, (*ranges_size)*8);
				if (!(*ranges))
					die("unrecoverable error: can't realloc"
					    "%d bytes for ranges.\n",
					    (*ranges_size)*8);
			}
			(*ranges)[idx++] = cpu_to_be64(loc_base);
			(*ranges)[idx++] = cpu_to_be64(loc_end - loc_base);

			rngs_cnt++;
		}
	}

	return rngs_cnt;
}

#ifdef HAVE_DYNAMIC_MEMORY
static void add_dyn_reconf_usable_mem_property__(int fd)
{
	char fname[MAXPATH], *bname;
	char buf[32];
	uint32_t lmbs_in_set = 1;
	uint64_t *ranges;
	int ranges_size = MEM_RANGE_CHUNK_SZ;
	uint64_t base, end, rngs_cnt;
	size_t i, j;
	int rlen = 0;
	int tmp_indx;

	strcpy(fname, pathname);
	bname = strrchr(fname, '/');
	bname[0] = '\0';
	bname = strrchr(fname, '/');
	if (strncmp(bname, "/ibm,dynamic-reconfiguration-memory", 36))
		return;

	if (lseek(fd, 4, SEEK_SET) < 0)
		die("unrecoverable error: error seeking in \"%s\": %s\n",
			pathname, strerror(errno));

	ranges = malloc(ranges_size*8);
	if (!ranges)
		die("unrecoverable error: can't alloc %d bytes for ranges.\n",
		    ranges_size*8);

	rlen = 0;
	for (i = 0; i < num_of_lmb_sets; i++) {
		if (read(fd, buf, LMB_ENTRY_SIZE) < 0)
			die("unrecoverable error: error reading \"%s\": %s\n",
				pathname, strerror(errno));

		/*
		 * If the property is ibm,dynamic-memory-v2, the first 4 bytes
		 * tell the number of sequential LMBs in this entry. Else, if
		 * the property is ibm,dynamic-memory, each entry represents
		 * one LMB. Make sure to add an entry for each LMB as kernel
		 * looks for a counter for every LMB.
		 */
		if (is_dyn_mem_v2)
			lmbs_in_set = be32_to_cpu(((unsigned int *)buf)[0]);

		base = be64_to_cpu(*((uint64_t *)&buf[DRCONF_ADDR]));
		for (j = 0; j < lmbs_in_set; j++) {
			end = base + lmb_size;
			if (~0ULL - base < end) {
				die("unrecoverable error: mem property"
				    " overflow\n");
			}

			tmp_indx = rlen++;

			rngs_cnt = add_ranges(&ranges, &ranges_size, rlen,
					      base, end);
			if (rngs_cnt == 0) {
				/* We still need to add a counter for every LMB
				 * because the kernel parsing code is dumb. We
				 * just have a zero in this case, with no
				 * following base/len.
				 */
				ranges[tmp_indx] = 0;

				/* rlen is already just tmp_indx+1 as we didn't
				 * write anything. Check array size here, as we
				 * will probably go on writing zeros for a while
				 */
				if (rlen >= (ranges_size-1)) {
					ranges_size += MEM_RANGE_CHUNK_SZ;
					ranges = realloc(ranges, ranges_size*8);
					if (!ranges)
						die("unrecoverable error: can't"
						    " realloc %d bytes for"
						    " ranges.\n",
						    ranges_size*8);
				}
			} else {
				/* Store the count of (base, size) duple */
				ranges[tmp_indx] = cpu_to_be64(rngs_cnt);
				rlen += rngs_cnt * 2;
			}

			base = end;
		}
	}
		
	rlen = rlen * sizeof(uint64_t);
	/*
	 * Add linux,drconf-usable-memory property.
	 */
	dt_reserve(&dt, 4+((rlen + 3)/4));
	*dt++ = cpu_to_be32(FDT_PROP);
	*dt++ = cpu_to_be32(rlen);
	*dt++ = cpu_to_be32(propnum("linux,drconf-usable-memory"));
	pad_structure_block(rlen);
	memcpy(dt, ranges, rlen);
	free(ranges);
	dt += (rlen + 3)/4;
}

static void add_dyn_reconf_usable_mem_property(struct dirent *dp, int fd)
{
	if ((!strcmp(dp->d_name, "ibm,dynamic-memory-v2") ||
	    !strcmp(dp->d_name, "ibm,dynamic-memory")) && usablemem_rgns.size)
		add_dyn_reconf_usable_mem_property__(fd);
}
#else
static void add_dyn_reconf_usable_mem_property(struct dirent *dp, int fd) {}
#endif

static void add_usable_mem_property(int fd, size_t len)
{
	char fname[MAXPATH], *bname;
	uint64_t buf[2];
	uint64_t *ranges;
	int ranges_size = MEM_RANGE_CHUNK_SZ;
	uint64_t base, end, rngs_cnt;
	int rlen = 0;

	strcpy(fname, pathname);
	bname = strrchr(fname,'/');
	bname[0] = '\0';
	bname = strrchr(fname,'/');
	if (strncmp(bname, "/memory@", 8) && strcmp(bname, "/memory"))
		return;

	if (len < sizeof(buf))
		die("unrecoverable error: not enough data for mem property\n");

	if (lseek(fd, 0, SEEK_SET) < 0)
		die("unrecoverable error: error seeking in \"%s\": %s\n",
		    pathname, strerror(errno));
	if (read(fd, buf, sizeof(buf)) != sizeof(buf))
		die("unrecoverable error: error reading \"%s\": %s\n",
		    pathname, strerror(errno));

	base = be64_to_cpu(buf[0]);
	end = be64_to_cpu(buf[1]);
	if (~0ULL - base < end)
		die("unrecoverable error: mem property overflow\n");
	end += base;

	ranges = malloc(ranges_size * sizeof(*ranges));
	if (!ranges)
		die("unrecoverable error: can't alloc %zu bytes for ranges.\n",
		    ranges_size * sizeof(*ranges));

	rngs_cnt = add_ranges(&ranges, &ranges_size, rlen, base, end);
	rlen += rngs_cnt * 2;

	if (!rlen) {
		/*
		 * User did not pass any ranges for thsi region. Hence, write
		 * (0,0) duple in linux,usable-memory property such that
		 * this region will be ignored.
		 */
		ranges[rlen++] = 0;
		ranges[rlen++] = 0;
	}

	rlen = rlen * sizeof(*ranges);
	/*
	 * No add linux,usable-memory property.
	 */
	dt_reserve(&dt, 4+((rlen + 3)/4));
	*dt++ = cpu_to_be32(FDT_PROP);
	*dt++ = cpu_to_be32(rlen);
	*dt++ = cpu_to_be32(propnum("linux,usable-memory"));
	pad_structure_block(rlen);
	memcpy(dt, ranges, rlen);
	free(ranges);
	dt += (rlen + 3)/4;
}

/* put all properties (files) in the property structure */
static void putprops(char *fn, struct dirent **nlist, int numlist)
{
	struct dirent *dp;
	int i = 0, fd;
	off_t len;
	off_t slen;
	struct stat statbuf;

	for (i = 0; i < numlist; i++) {
		dp = nlist[i];
		strcpy(fn, dp->d_name);

		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
                        continue;

		/* Empirically, this seems to need to be ecluded.
		 * Observed on ARM with 3.6-rc2 kernel
		 */
		if (!strcmp(dp->d_name, "name"))
                        continue;

		if (lstat(pathname, &statbuf))
			die("unrecoverable error: could not stat \"%s\": %s\n",
			    pathname, strerror(errno));

		if (!crash_param && !strcmp(fn,"linux,crashkernel-base"))
			continue;

		if (!crash_param && !strcmp(fn,"linux,crashkernel-size"))
			continue;

		/*
		 * This property will be created for each node during kexec
		 * boot. So, ignore it.
		 */
		if (!strcmp(dp->d_name, "linux,pci-domain") ||
			!strcmp(dp->d_name, "linux,htab-base") ||
			!strcmp(dp->d_name, "linux,htab-size") ||
			!strcmp(dp->d_name, "linux,kernel-end"))
				continue;

		/* This property will be created/modified later in putnode()
		 * So ignore it, unless we are reusing the initrd.
		 */
		if ((!strcmp(dp->d_name, "linux,initrd-start") ||
		     !strcmp(dp->d_name, "linux,initrd-end")) &&
		    !reuse_initrd)
				continue;

		/* This property will be created later in putnode() So
		 * ignore it now.
		 */
		if (!strcmp(dp->d_name, "bootargs"))
			continue;

		if (! S_ISREG(statbuf.st_mode))
			continue;

		len = statbuf.st_size;

		dt_reserve(&dt, 4+((len + 3)/4));
		*dt++ = cpu_to_be32(FDT_PROP);
		*dt++ = cpu_to_be32(len);
		*dt++ = cpu_to_be32(propnum(fn));
		pad_structure_block(len);

		if (len) {
			char *buf;

			buf = slurp_file_len(pathname, len, &slen);
			if (slen != len)
				die("unrecoverable error: short read from\"%s\"\n",
				    pathname);

			memcpy(dt, buf, slen);
			free(buf);
		}

		checkprop(fn, dt, len);

		dt += (len + 3)/4;

		fd = open(pathname, O_RDONLY);
		if (fd == -1)
			die("unrecoverable error: could not open \"%s\": %s\n",
			    pathname, strerror(errno));

		if (!strcmp(dp->d_name, "reg") && usablemem_rgns.size)
			add_usable_mem_property(fd, len);
		add_dyn_reconf_usable_mem_property(dp, fd);
		close(fd);
	}

	fn[0] = '\0';
	checkprop(pathname, NULL, 0);
}

/*
 * Compare function used to sort the device-tree directories
 * This function will be passed to scandir.
 */
static int comparefunc(const struct dirent **dentry1,
		       const struct dirent **dentry2)
{
	char *str1 = (*(struct dirent **)dentry1)->d_name;
	char *str2 = (*(struct dirent **)dentry2)->d_name;
	char *sep1 = strchr(str1, '@');
	char *sep2 = strchr(str2, '@');

	/*
	 * strcmp scans from left to right and fails to idetify for some
	 * strings such as memory@10000000 and memory@f000000.
	 * Therefore, we get the wrong sorted order like memory@10000000 and
	 * memory@f000000.
	 */
	if (sep1 && sep2) {
		int baselen1 = sep1 - str1;
		int baselen2 = sep2 - str2;
		int len1 = strlen(str1);
		int len2 = strlen(str2);

		/*
		 * Check the base name matches, and the properties are
		 * different lengths.
		 */
		if ((baselen1 == baselen2) && (len1 != len2) &&
		    !strncmp(str1, str2, baselen2))
			return (len1 > len2) - (len1 < len2);
	}

	return strcmp(str1, str2);
}

/* grab root= from the old command line */
static void dt_copy_old_root_param(void)
{
	FILE *fp;
	char filename[MAXPATH];
	char *last_cmdline = NULL;
	char *p, *old_param;
	size_t len = 0;

	strcpy(filename, pathname);
	strcat(filename, "bootargs");
	fp = fopen(filename, "r");
	if (!fp)
		return;

	if (getline(&last_cmdline, &len, fp) == -1)
		die("unable to read %s\n", filename);

	p = strstr(last_cmdline, "root=");
	if (p) {
		old_param = strtok(p, " ");
		len = strlen(local_cmdline);
		if (len != 0)
			strcat(local_cmdline, " ");
		strcat(local_cmdline, old_param);
	}

	if (last_cmdline)
		free(last_cmdline);

	fclose(fp);
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
	int plen, ret;

	numlist = scandir(pathname, &namelist, 0, comparefunc);
	if (numlist < 0)
		die("unrecoverable error: could not scan \"%s\": %s\n",
		    pathname, strerror(errno));
	if (numlist == 0)
		die("unrecoverable error: no directory entries in \"%s\"",
		    pathname);

	basename = strrchr(pathname,'/') + 1;

	plen = *basename ? strlen(basename) : 0;
	/* Reserve space for string packed to words; e.g. string length 10
	 * occupies 3 words, length 12 occupies 4 (for terminating \0s).
	 * So round up & include the \0:
	 */
	dt_reserve(&dt, 1+((plen + 4)/4));
	*dt++ = cpu_to_be32(FDT_BEGIN_NODE);
	strcpy((void *)dt, *basename ? basename : "");
	dt += ((plen + 4)/4);

	if (*basename)
		strcat(pathname, "/");
	dn = pathname + strlen(pathname);

	putprops(dn, namelist, numlist);

	/* Add initrd entries to the second kernel */
	if (initrd_base && initrd_size && !strcmp(basename,"chosen/")) {
		int len = 8;
		uint64_t bevalue;

		dt_reserve(&dt, 12); /* both props, of 6 words ea. */
		*dt++ = cpu_to_be32(FDT_PROP);
		*dt++ = cpu_to_be32(len);
		*dt++ = cpu_to_be32(propnum("linux,initrd-start"));
		pad_structure_block(len);

		bevalue = cpu_to_be64(initrd_base);
		memcpy(dt, &bevalue, len);
		dt += (len + 3)/4;

		len = 8;
		*dt++ = cpu_to_be32(FDT_PROP);
		*dt++ = cpu_to_be32(len);
		*dt++ = cpu_to_be32(propnum("linux,initrd-end"));

		bevalue = cpu_to_be64(initrd_base + initrd_size);
		pad_structure_block(len);

		memcpy(dt, &bevalue, len);
		dt += (len + 3)/4;

		reserve(initrd_base, initrd_size);
	}

	/*
	 * Add cmdline to the second kernel. Use the old root= cmdline if there
	 * is no root= in the new command line and there's no --dt-no-old-root
	 * option being used.
	 */
	if (!strcmp(basename,"chosen/")) {
		size_t result;
		size_t cmd_len = 0;
		char *param = NULL;
		char filename[MAXPATH];
		char *buff;
		int fd;

		cmd_len = strlen(local_cmdline);
		if (cmd_len != 0) {
			param = strstr(local_cmdline, "crashkernel=");
			if (param)
				crash_param = 1;
			/* does the new cmdline have a root= ? ... */
			param = strstr(local_cmdline, "root=");
		}

		if (!param && !dt_no_old_root)
			dt_copy_old_root_param();

		strcat(local_cmdline, " ");
		cmd_len = strlen(local_cmdline);
		cmd_len = cmd_len + 1;

		/* add new bootargs */
		dt_reserve(&dt, 4+((cmd_len+3)/4));
		*dt++ = cpu_to_be32(FDT_PROP);
		*dt++ = cpu_to_be32(cmd_len);
		*dt++ = cpu_to_be32(propnum("bootargs"));
		pad_structure_block(cmd_len);
		memcpy(dt, local_cmdline,cmd_len);
		dt += (cmd_len + 3)/4;

		fprintf(stderr, "Modified cmdline:%s\n", local_cmdline);

		/*
		 * Determine the platform type/stdout type, so that purgatory
		 * code can print 'I'm in purgatory' message. Currently only
		 * pseries/hvcterminal is supported.
		 */
		ret = snprintf(filename, MAXPATH, "%sstdout-path", pathname);
		if (ret < 0 || ret >= MAXPATH) {
			fprintf(stderr, "snprintf failed: %s\n", strerror(errno));
			goto no_debug;
		}

		fd = open(filename, O_RDONLY);
		if (fd == -1) {
			ret = snprintf(filename, MAXPATH, "%slinux,stdout-path", pathname);
			if (ret < 0 || ret >= MAXPATH) {
				fprintf(stderr, "snprintf failed: %s\n", strerror(errno));
				goto no_debug;
			}

			fd = open(filename, O_RDONLY);
			if (fd == -1) {
				printf("Unable to find %s[linux,]stdout-path, printing from purgatory is disabled\n",
														pathname);
				goto no_debug;
			}
		}
		if (fstat(fd, &statbuf)) {
			printf("Unable to stat %s, printing from purgatory is disabled\n",
														filename);
			close(fd);
			goto no_debug;

		}

		buff = malloc(statbuf.st_size);
		if (!buff) {
			printf("Can not allocate memory for buff\n");
			close(fd);
			goto no_debug;
		}
		result = read(fd, buff, statbuf.st_size);
		close(fd);
		if (result <= 0) {
			printf("Unable to read %s, printing from purgatory is disabled\n",
														filename);
			goto no_debug;
		}
		snprintf(filename, MAXPATH, "/proc/device-tree/%s/compatible", buff);
		fd = open(filename, O_RDONLY);
		if (fd == -1) {
			printf("Unable to find %s printing from purgatory is disabled\n",
														filename);
			goto no_debug;
		}
		if (fstat(fd, &statbuf)) {
			printf("Unable to stat %s printing from purgatory is disabled\n",
														filename);
			close(fd);
			goto no_debug;
		}
		buff = realloc(buff, statbuf.st_size);
		if (!buff) {
			printf("Can not allocate memory for buff\n");
			close(fd);
			goto no_debug;
		}
		result = read(fd, buff, statbuf.st_size);
		if (result && (!strcmp(buff, "hvterm1")
			|| !strcmp(buff, "hvterm-protocol")))
			my_debug = 1;
		close(fd);
		free(buff);
	}

no_debug:
	for (i=0; i < numlist; i++) {
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

	dt_reserve(&dt, 1);
	*dt++ = cpu_to_be32(FDT_END_NODE);
	dn[-1] = '\0';
	free(namelist);
}

struct bootblock bb[1];

static void add_boot_block(char **bufp, off_t *sizep)
{
	unsigned long len;
	unsigned long tlen, toff;
	char *buf;

	len = _ALIGN(sizeof(bb[0]), 8);

	bb->off_mem_rsvmap = cpu_to_be32(len);

	for (len = 1; be64_to_cpu(mem_rsrv[len]); len += 2)
		;
	len++;
#ifdef NEED_RESERVE_DTB
	len+= 3; /* Leave space for totalsize reservation */
#endif
	len *= sizeof(mem_rsrv[0]);

	bb->off_dt_struct = cpu_to_be32(be32_to_cpu(bb->off_mem_rsvmap) + len);

	len = dt - dt_base;
	len *= sizeof(unsigned);
#if (BOOT_BLOCK_VERSION >= 17)
	bb->dt_struct_size = cpu_to_be32(len);
#endif
	bb->off_dt_strings = cpu_to_be32(be32_to_cpu(bb->off_dt_struct) + len);

	len = propnum("");
	bb->dt_strings_size = cpu_to_be32(len);
	len = _ALIGN(len, 4);
	bb->totalsize = cpu_to_be32(be32_to_cpu(bb->off_dt_strings) + len);

	bb->magic = cpu_to_be32(FDT_MAGIC);
	bb->version = cpu_to_be32(BOOT_BLOCK_VERSION);
	bb->last_comp_version = cpu_to_be32(BOOT_BLOCK_LAST_COMP_VERSION);

#ifdef NEED_RESERVE_DTB
	reserve(0, be32_to_cpu(bb->totalsize)); /* patched later in kexec_load */
#endif

	buf = malloc(be32_to_cpu(bb->totalsize));
	*bufp = buf;

	tlen = be32_to_cpu(bb->off_mem_rsvmap);
	memcpy(buf, bb, tlen);

	toff = be32_to_cpu(bb->off_mem_rsvmap);
	tlen = be32_to_cpu(bb->off_dt_struct) - be32_to_cpu(bb->off_mem_rsvmap);
	memcpy(buf + toff, mem_rsrv, tlen);

	toff += be32_to_cpu(bb->off_dt_struct) - be32_to_cpu(bb->off_mem_rsvmap);
	tlen = be32_to_cpu(bb->off_dt_strings) - be32_to_cpu(bb->off_dt_struct);
	memcpy(buf + toff, dt_base,  tlen);

	toff += be32_to_cpu(bb->off_dt_strings) - be32_to_cpu(bb->off_dt_struct);
	tlen = be32_to_cpu(bb->totalsize) - be32_to_cpu(bb->off_dt_strings);
	memcpy(buf + toff, propnames,  tlen);

	*sizep = toff + be32_to_cpu(bb->totalsize) -
		be32_to_cpu(bb->off_dt_strings);
}

void create_flatten_tree(char **bufp, off_t *sizep, const char *cmdline)
{
	strcpy(pathname, "/proc/device-tree/");

	dt_cur_size = INIT_TREE_WORDS;
	dt_base = malloc(dt_cur_size*4);
	if (!dt_base) {
		die("Can't malloc %d bytes for dt struct!\n", dt_cur_size*4);
	}
	memset(dt_base, 0, dt_cur_size*4);

	dt = dt_base;

	if (cmdline)
		strcpy(local_cmdline, cmdline);

	putnode();
	dt_reserve(&dt, 1);
	*dt++ = cpu_to_be32(FDT_END);

	add_boot_block(bufp, sizep);
	free(dt_base);
}
