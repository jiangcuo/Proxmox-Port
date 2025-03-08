/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2003-2005  Eric Biederman (ebiederm@xmission.com)
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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <libfdt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <getopt.h>
#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "kexec-ppc64.h"
#include "../../fs2dt.h"
#include "crashdump-ppc64.h"
#include <arch/options.h>

static struct memory_range *exclude_range = NULL;
static struct memory_range *memory_range = NULL;
static struct memory_range *base_memory_range = NULL;
static uint64_t rma_top;
uint64_t memory_max = 0;
uint64_t memory_limit;
static int nr_memory_ranges, nr_exclude_ranges;
uint64_t crash_base, crash_size;
unsigned int rtas_base, rtas_size;
uint64_t opal_base, opal_size;
int max_memory_ranges;

static void cleanup_memory_ranges(void)
{
	if (memory_range)
		free(memory_range);
	if (base_memory_range)
		free(base_memory_range);
	if (exclude_range)
		free(exclude_range);
	if (usablemem_rgns.ranges)
		free(usablemem_rgns.ranges);
}

/*
 * Allocate memory for various data structures used to hold
 * values of different memory ranges
 */
static int alloc_memory_ranges(void)
{
	int memory_range_len;

	memory_range_len = sizeof(struct memory_range) * max_memory_ranges;

	memory_range = (struct memory_range *) malloc(memory_range_len);
	if (!memory_range)
		return -1;

	base_memory_range = (struct memory_range *) malloc(memory_range_len);
	if (!base_memory_range)
		goto err1;

	exclude_range = (struct memory_range *) malloc(memory_range_len);
	if (!exclude_range)
		goto err1;

	usablemem_rgns.ranges = (struct memory_range *)
				malloc(memory_range_len);
	if (!(usablemem_rgns.ranges))
		goto err1;

	memset(memory_range, 0, memory_range_len);
	memset(base_memory_range, 0, memory_range_len);
	memset(exclude_range, 0, memory_range_len);
	memset(usablemem_rgns.ranges, 0, memory_range_len);
	return 0;

err1:
	fprintf(stderr, "memory range structure allocation failure\n");
	cleanup_memory_ranges();
	return -1;

}

static int realloc_memory_ranges(void)
{
	size_t memory_range_len;

	max_memory_ranges++;
	memory_range_len = sizeof(struct memory_range) * max_memory_ranges;

	memory_range = (struct memory_range *) realloc(memory_range, memory_range_len);
	if (!memory_range)
		goto err;

	base_memory_range = (struct memory_range *) realloc(base_memory_range, memory_range_len);
	if (!base_memory_range)
		goto err;

	exclude_range = (struct memory_range *) realloc(exclude_range, memory_range_len);
	if (!exclude_range)
		goto err;

	usablemem_rgns.ranges = (struct memory_range *)
				realloc(usablemem_rgns.ranges, memory_range_len);
	if (!(usablemem_rgns.ranges))
		goto err;

	return 0;

err:
	fprintf(stderr, "memory range structure re-allocation failure\n");
	return -1;
}


static void add_base_memory_range(uint64_t start, uint64_t end)
{
	base_memory_range[nr_memory_ranges].start = start;
	base_memory_range[nr_memory_ranges].end  = end;
	base_memory_range[nr_memory_ranges].type = RANGE_RAM;
	nr_memory_ranges++;
	if (nr_memory_ranges >= max_memory_ranges)
		realloc_memory_ranges();

	dbgprintf("%016llx-%016llx : %x\n",
		base_memory_range[nr_memory_ranges-1].start,
		base_memory_range[nr_memory_ranges-1].end,
		base_memory_range[nr_memory_ranges-1].type);
}

static int get_dyn_reconf_base_ranges(void)
{
	uint64_t start, end;
	uint64_t size;
	char fname[128], buf[32];
	FILE *file;
	unsigned int i;
	int n;

	strcpy(fname, "/proc/device-tree/");
	strcat(fname, "ibm,dynamic-reconfiguration-memory/ibm,lmb-size");
	if ((file = fopen(fname, "r")) == NULL) {
		perror(fname);
		return -1;
	}
	if (fread(buf, 1, 8, file) != 8) {
		perror(fname);
		fclose(file);
		return -1;
	}
	/*
	 * lmb_size, num_of_lmb_sets(global variables) are
	 * initialized once here.
	 */
	size = lmb_size = be64_to_cpu(((uint64_t *)buf)[0]);
	fclose(file);

	strcpy(fname, "/proc/device-tree/");
	strcat(fname,
		"ibm,dynamic-reconfiguration-memory/ibm,dynamic-memory");
	if ((file = fopen(fname, "r")) == NULL) {
		strcat(fname, "-v2");
		if ((file = fopen(fname, "r")) == NULL) {
			perror(fname);
			return -1;
		}

		is_dyn_mem_v2 = 1;
	}

	/* first 4 bytes tell the number of lmb set entries */
	if (fread(buf, 1, 4, file) != 4) {
		perror(fname);
		fclose(file);
		return -1;
	}
	num_of_lmb_sets = be32_to_cpu(((unsigned int *)buf)[0]);

	for (i = 0; i < num_of_lmb_sets; i++) {
		if ((n = fread(buf, 1, LMB_ENTRY_SIZE, file)) < 0) {
			perror(fname);
			fclose(file);
			return -1;
		}
		if (nr_memory_ranges >= max_memory_ranges) {
			fclose(file);
			return -1;
		}

		/*
		 * If the property is ibm,dynamic-memory-v2, the first 4 bytes
		 * tell the number of sequential LMBs in this entry.
		 */
		if (is_dyn_mem_v2)
			size = be32_to_cpu(((unsigned int *)buf)[0]) * lmb_size;

		start = be64_to_cpu(*((uint64_t *)&buf[DRCONF_ADDR]));
		end = start + size;
		add_base_memory_range(start, end);
	}
	fclose(file);
	return 0;
}

/* Sort the base ranges in memory - this is useful for ensuring that our
 * ranges are in ascending order, even if device-tree read of memory nodes
 * is done differently. Also, could be used for other range coalescing later
 */
static int sort_base_ranges(void)
{
	int i, j;
	unsigned long long tstart, tend;

	for (i = 0; i < nr_memory_ranges - 1; i++) {
		for (j = 0; j < nr_memory_ranges - i - 1; j++) {
			if (base_memory_range[j].start > base_memory_range[j+1].start) {
				tstart = base_memory_range[j].start;
				tend = base_memory_range[j].end;
				base_memory_range[j].start = base_memory_range[j+1].start;
				base_memory_range[j].end = base_memory_range[j+1].end;
				base_memory_range[j+1].start = tstart;
				base_memory_range[j+1].end = tend;
			}
		}
	}
	return 0;
}

/* Get base memory ranges */
static int get_base_ranges(void)
{
	uint64_t start, end;
	char device_tree[256] = "/proc/device-tree/";
	char fname[256];
	char buf[MAXBYTES];
	DIR *dir, *dmem;
	FILE *file;
	struct dirent *dentry, *mentry;
	int n;

	if ((dir = opendir(device_tree)) == NULL) {
		perror(device_tree);
		return -1;
	}
	while ((dentry = readdir(dir)) != NULL) {
		if (!strncmp(dentry->d_name,
				"ibm,dynamic-reconfiguration-memory", 35)) {
			get_dyn_reconf_base_ranges();
			continue;
		}
		if (strncmp(dentry->d_name, "memory@", 7) &&
			strcmp(dentry->d_name, "memory"))
			continue;
		strcpy(fname, device_tree);
		strcat(fname, dentry->d_name);
		if ((dmem = opendir(fname)) == NULL) {
			perror(fname);
			closedir(dir);
			return -1;
		}
		while ((mentry = readdir(dmem)) != NULL) {
			if (strcmp(mentry->d_name, "reg"))
				continue;
			strcat(fname, "/reg");
			if ((file = fopen(fname, "r")) == NULL) {
				perror(fname);
				closedir(dmem);
				closedir(dir);
				return -1;
			}
			if ((n = fread(buf, 1, MAXBYTES, file)) < 0) {
				perror(fname);
				fclose(file);
				closedir(dmem);
				closedir(dir);
				return -1;
			}
			if (nr_memory_ranges >= max_memory_ranges) {
				if (realloc_memory_ranges() < 0)
					break;
			}
			start =  be64_to_cpu(((uint64_t *)buf)[0]);
			end = start + be64_to_cpu(((uint64_t *)buf)[1]);
			add_base_memory_range(start, end);
			fclose(file);
		}
		closedir(dmem);
	}
	closedir(dir);
	sort_base_ranges();
	memory_max = base_memory_range[nr_memory_ranges - 1].end;
	dbgprintf("get base memory ranges:%d\n", nr_memory_ranges);

	return 0;
}

/* Sort the exclude ranges in memory */
static int sort_ranges(void)
{
	int i, j;
	uint64_t tstart, tend;
	for (i = 0; i < nr_exclude_ranges - 1; i++) {
		for (j = 0; j < nr_exclude_ranges - i - 1; j++) {
			if (exclude_range[j].start > exclude_range[j+1].start) {
				tstart = exclude_range[j].start;
				tend = exclude_range[j].end;
				exclude_range[j].start = exclude_range[j+1].start;
				exclude_range[j].end = exclude_range[j+1].end;
				exclude_range[j+1].start = tstart;
				exclude_range[j+1].end = tend;
			}
		}
	}
	return 0;
}

void scan_reserved_ranges(unsigned long kexec_flags, int *range_index)
{
	char fname[256], buf[16];
	FILE *file;
	int i = *range_index;

	strcpy(fname, "/proc/device-tree/reserved-ranges");

	file = fopen(fname, "r");
	if (file == NULL) {
		if (errno != ENOENT) {
			perror(fname);
			return;
		}
		errno = 0;
		/* File not present. Non PowerKVM system. */
		return;
	}

	/*
	 * Each reserved range is an (address,size) pair, 2 cells each,
	 * totalling 4 cells per range.
	 */
	while (fread(buf, sizeof(uint64_t) * 2, 1, file) == 1) {
		uint64_t base, size;

		base = be64_to_cpu(((uint64_t *)buf)[0]);
		size = be64_to_cpu(((uint64_t *)buf)[1]);

		exclude_range[i].start = base;
		exclude_range[i].end = base + size;
		i++;
		if (i >= max_memory_ranges)
			realloc_memory_ranges();

		reserve(base, size);
	}
	fclose(file);
	*range_index = i;
}

/* Return 0 if fname/value valid, -1 otherwise */
int get_devtree_value(const char *fname, unsigned long long *value)
{
	FILE *file;
	char buf[MAXBYTES];
	int n = -1;

	if ((file = fopen(fname, "r"))) {
		n = fread(buf, 1, MAXBYTES, file);
		fclose(file);
	}

	if (n == sizeof(uint32_t))
		*value = ((uint32_t *)buf)[0];
	else if (n == sizeof(uint64_t))
		*value = ((uint64_t *)buf)[0];
	else {
		fprintf(stderr, "%s node has invalid size: %d\n", fname, n);
		return -1;
	}

	return 0;
}

/* Get devtree details and create exclude_range array
 * Also create usablemem_ranges for KEXEC_ON_CRASH
 */
static int get_devtree_details(unsigned long kexec_flags)
{
	uint64_t rma_base = -1, base;
	uint64_t tce_base;
	unsigned int tce_size;
	uint64_t htab_base, htab_size;
	uint64_t kernel_end;
	uint64_t initrd_start, initrd_end;
	char buf[MAXBYTES];
	char device_tree[256] = "/proc/device-tree/";
	char fname[256];
	DIR *dir, *cdir;
	FILE *file;
	struct dirent *dentry;
	struct stat fstat;
	int n, i = 0;

	if ((dir = opendir(device_tree)) == NULL) {
		perror(device_tree);
		return -1;
	}

	scan_reserved_ranges(kexec_flags, &i);

	while ((dentry = readdir(dir)) != NULL) {
		if (strncmp(dentry->d_name, "chosen", 6) &&
			strncmp(dentry->d_name, "memory@", 7) &&
			strcmp(dentry->d_name, "memory") &&
			strncmp(dentry->d_name, "pci@", 4) &&
			strncmp(dentry->d_name, "rtas", 4) &&
			strncmp(dentry->d_name, "ibm,opal", 8))
			continue;
		strcpy(fname, device_tree);
		strcat(fname, dentry->d_name);
		if ((cdir = opendir(fname)) == NULL) {
			perror(fname);
			goto error_opendir;
		}

		if (strncmp(dentry->d_name, "chosen", 6) == 0) {
			strcat(fname, "/linux,kernel-end");
			if ((file = fopen(fname, "r")) == NULL) {
				perror(fname);
				goto error_opencdir;
			}
			if (fread(&kernel_end, sizeof(uint64_t), 1, file) != 1) {
				perror(fname);
				goto error_openfile;
			}
			fclose(file);
			kernel_end = be64_to_cpu(kernel_end);

			/* Add kernel memory to exclude_range */
			exclude_range[i].start = 0x0UL;
			exclude_range[i].end = kernel_end;
			i++;
			if (i >= max_memory_ranges)
				realloc_memory_ranges();

			if (kexec_flags & KEXEC_ON_CRASH) {
				memset(fname, 0, sizeof(fname));
				strcpy(fname, device_tree);
				strcat(fname, dentry->d_name);
				strcat(fname, "/linux,crashkernel-base");
				if ((file = fopen(fname, "r")) == NULL) {
					perror(fname);
					goto error_opencdir;
				}
				if (fread(&crash_base, sizeof(uint64_t), 1,
						file) != 1) {
					perror(fname);
					goto error_openfile;
				}
				fclose(file);
				crash_base = be64_to_cpu(crash_base);

				memset(fname, 0, sizeof(fname));
				strcpy(fname, device_tree);
				strcat(fname, dentry->d_name);
				strcat(fname, "/linux,crashkernel-size");
				if ((file = fopen(fname, "r")) == NULL) {
					perror(fname);
					goto error_opencdir;
				}
				if (fread(&crash_size, sizeof(uint64_t), 1,
						file) != 1) {
					perror(fname);
					goto error_openfile;
				}
				fclose(file);
				crash_size = be64_to_cpu(crash_size);

				if (crash_base > mem_min)
					mem_min = crash_base;
				if (crash_base + crash_size < mem_max)
					mem_max = crash_base + crash_size;
				
				add_usable_mem_rgns(0, crash_base + crash_size);
				reserve(KDUMP_BACKUP_LIMIT, crash_base-KDUMP_BACKUP_LIMIT);
			}
			/*
			 * Read the first kernel's memory limit.
			 * If the first kernel is booted with mem= option then
			 * it would export "linux,memory-limit" file
			 * reflecting value for the same.
			 */
			memset(fname, 0, sizeof(fname));
			strcpy(fname, device_tree);
			strcat(fname, dentry->d_name);
			strcat(fname, "/linux,memory-limit");
			if ((file = fopen(fname, "r")) == NULL) {
				if (errno != ENOENT) {
					perror(fname);
					goto error_opencdir;
				}
				errno = 0;
				/*
				 * File not present.
				 * fall through. On older kernel this file
				 * is not present.
				 */
			} else {
				if (fread(&memory_limit, sizeof(uint64_t), 1,
					  file) != 1) {
					perror(fname);
					goto error_openfile;
				}
				fclose(file);
				memory_limit = be64_to_cpu(memory_limit);
			}

			memset(fname, 0, sizeof(fname));
			strcpy(fname, device_tree);
			strcat(fname, dentry->d_name);
			strcat(fname, "/linux,htab-base");
			if ((file = fopen(fname, "r")) == NULL) {
				closedir(cdir);
				if (errno == ENOENT) {
					/* Non LPAR */
					errno = 0;
					continue;
                                }
				perror(fname);
				goto error_opendir;
			}
			if (fread(&htab_base, sizeof(uint64_t), 1, file) != 1) {
				perror(fname);
				goto error_openfile;
			}
			fclose(file);
			htab_base = be64_to_cpu(htab_base);

			memset(fname, 0, sizeof(fname));
			strcpy(fname, device_tree);
			strcat(fname, dentry->d_name);
			strcat(fname, "/linux,htab-size");
			if ((file = fopen(fname, "r")) == NULL) {
				perror(fname);
				goto error_opencdir;
			}
			if (fread(&htab_size, sizeof(uint64_t), 1, file) != 1) {
				perror(fname);
				goto error_openfile;
			}
			fclose(file);
			htab_size = be64_to_cpu(htab_size);

			/* Add htab address to exclude_range - NON-LPAR only */
			exclude_range[i].start = htab_base;
			exclude_range[i].end = htab_base + htab_size;
			i++;
			if (i >= max_memory_ranges)
				realloc_memory_ranges();

			/* reserve the initrd_start and end locations. */
			if (reuse_initrd) {
				memset(fname, 0, sizeof(fname));
				strcpy(fname, device_tree);
				strcat(fname, dentry->d_name);
				strcat(fname, "/linux,initrd-start");
				if ((file = fopen(fname, "r")) == NULL) {
					perror(fname);
					goto error_opencdir;
				}
				/* check for 4 and 8 byte initrd offset sizes */
				if (stat(fname, &fstat) != 0) {
					perror(fname);
					goto error_openfile;
				}
				if (fread(&initrd_start, fstat.st_size, 1, file) != 1) {
					perror(fname);
					goto error_openfile;
				}
				initrd_start = be64_to_cpu(initrd_start);
				fclose(file);

				memset(fname, 0, sizeof(fname));
				strcpy(fname, device_tree);
				strcat(fname, dentry->d_name);
				strcat(fname, "/linux,initrd-end");
				if ((file = fopen(fname, "r")) == NULL) {
					perror(fname);
					goto error_opencdir;
				}
				/* check for 4 and 8 byte initrd offset sizes */
				if (stat(fname, &fstat) != 0) {
					perror(fname);
					goto error_openfile;
				}
				if (fread(&initrd_end, fstat.st_size, 1, file) != 1) {
					perror(fname);
					goto error_openfile;
				}
				initrd_end = be64_to_cpu(initrd_end);
				fclose(file);

				/* Add initrd address to exclude_range */
				exclude_range[i].start = initrd_start;
				exclude_range[i].end = initrd_end;
				i++;
				if (i >= max_memory_ranges)
					realloc_memory_ranges();
			}
		} /* chosen */

		if (strncmp(dentry->d_name, "rtas", 4) == 0) {
			strcat(fname, "/linux,rtas-base");
			if ((file = fopen(fname, "r")) == NULL) {
				perror(fname);
				goto error_opencdir;
			}
			if (fread(&rtas_base, sizeof(unsigned int), 1, file) != 1) {
				perror(fname);
				goto error_openfile;
			}
			fclose(file);
			rtas_base = be32_to_cpu(rtas_base);
			memset(fname, 0, sizeof(fname));
			strcpy(fname, device_tree);
			strcat(fname, dentry->d_name);
			strcat(fname, "/rtas-size");
			if ((file = fopen(fname, "r")) == NULL) {
				perror(fname);
				goto error_opencdir;
			}
			if (fread(&rtas_size, sizeof(unsigned int), 1, file) != 1) {
				perror(fname);
				goto error_openfile;
			}
			fclose(file);
			closedir(cdir);
			rtas_size = be32_to_cpu(rtas_size);
			/* Add rtas to exclude_range */
			exclude_range[i].start = rtas_base;
			exclude_range[i].end = rtas_base + rtas_size;
			i++;
			if (i >= max_memory_ranges)
				realloc_memory_ranges();
			if (kexec_flags & KEXEC_ON_CRASH)
				add_usable_mem_rgns(rtas_base, rtas_size);
		} /* rtas */

		if (strncmp(dentry->d_name, "ibm,opal", 8) == 0) {
			strcat(fname, "/opal-base-address");
			file = fopen(fname, "r");
			if (file == NULL) {
				perror(fname);
				goto error_opencdir;
			}
			if (fread(&opal_base, sizeof(uint64_t), 1, file) != 1) {
				perror(fname);
				goto error_openfile;
			}
			opal_base = be64_to_cpu(opal_base);
			fclose(file);

			memset(fname, 0, sizeof(fname));
			strcpy(fname, device_tree);
			strcat(fname, dentry->d_name);
			strcat(fname, "/opal-runtime-size");
			file = fopen(fname, "r");
			if (file == NULL) {
				perror(fname);
				goto error_opencdir;
			}
			if (fread(&opal_size, sizeof(uint64_t), 1, file) != 1) {
				perror(fname);
				goto error_openfile;
			}
			fclose(file);
			closedir(cdir);
			opal_size = be64_to_cpu(opal_size);
			/* Add OPAL to exclude_range */
			exclude_range[i].start = opal_base;
			exclude_range[i].end = opal_base + opal_size;
			i++;
			if (i >= max_memory_ranges)
				realloc_memory_ranges();
			if (kexec_flags & KEXEC_ON_CRASH)
				add_usable_mem_rgns(opal_base, opal_size);
		} /* ibm,opal */

		if (!strncmp(dentry->d_name, "memory@", 7) ||
			!strcmp(dentry->d_name, "memory")) {
			strcat(fname, "/reg");
			if ((file = fopen(fname, "r")) == NULL) {
				perror(fname);
				goto error_opencdir;
			}
			if ((n = fread(buf, 1, MAXBYTES, file)) < 0) {
				perror(fname);
				goto error_openfile;
			}
			base = be64_to_cpu(((uint64_t *)buf)[0]);
			if (base < rma_base) {
				rma_base = base;
				rma_top = base + be64_to_cpu(((uint64_t *)buf)[1]);
			}

			fclose(file);
			closedir(cdir);
		} /* memory */

		if (strncmp(dentry->d_name, "pci@", 4) == 0) {
			strcat(fname, "/linux,tce-base");
			if ((file = fopen(fname, "r")) == NULL) {
				closedir(cdir);
				if (errno == ENOENT) {
					/* Non LPAR */
					errno = 0;
					continue;
				}
				perror(fname);
				goto error_opendir;
			}
			if (fread(&tce_base, sizeof(uint64_t), 1, file) != 1) {
				perror(fname);
				goto error_openfile;
			}
			fclose(file);
			tce_base = be64_to_cpu(tce_base);
			memset(fname, 0, sizeof(fname));
			strcpy(fname, device_tree);
			strcat(fname, dentry->d_name);
			strcat(fname, "/linux,tce-size");
			if ((file = fopen(fname, "r")) == NULL) {
				perror(fname);
				goto error_opencdir;
			}
			if (fread(&tce_size, sizeof(unsigned int), 1, file) != 1) {
				perror(fname);
				goto error_openfile;
			}
			fclose(file);
			tce_size = be32_to_cpu(tce_size);
			/* Add tce to exclude_range - NON-LPAR only */
			exclude_range[i].start = tce_base;
			exclude_range[i].end = tce_base + tce_size;
			i++;
			if (i >= max_memory_ranges)
				realloc_memory_ranges();
			if (kexec_flags & KEXEC_ON_CRASH)
				add_usable_mem_rgns(tce_base, tce_size);
			closedir(cdir);
		} /* pci */
	}
	closedir(dir);

	nr_exclude_ranges = i;

	sort_ranges();

	int k;
	for (k = 0; k < i; k++)
		dbgprintf("exclude_range sorted exclude_range[%d] "
			"start:%llx, end:%llx\n", k, exclude_range[k].start,
			exclude_range[k].end);

	return 0;

error_openfile:
	fclose(file);
error_opencdir:
	closedir(cdir);
error_opendir:
	closedir(dir);
	return -1;
}

/* Setup a sorted list of memory ranges. */
int setup_memory_ranges(unsigned long kexec_flags)
{
	int i, j = 0;

	/* Get the base list of memory ranges from /proc/device-tree/memory
	 * nodes. Build list of ranges to be excluded from valid memory
	 */

	if (get_base_ranges())
		goto out;
	if (get_devtree_details(kexec_flags))
		goto out;

	for (i = 0; i < nr_exclude_ranges; i++) {
		/* If first exclude range does not start with 0, include the
		 * first hole of valid memory from 0 - exclude_range[0].start
		 */
		if (i == 0) {
			if (exclude_range[i].start != 0) {
				memory_range[j].start = 0;
				memory_range[j].end = exclude_range[i].start - 1;
				memory_range[j].type = RANGE_RAM;
				j++;
				if (j >= max_memory_ranges)
					realloc_memory_ranges();
			}
		} /* i == 0 */
		/* If the last exclude range does not end at memory_max, include
		 * the last hole of valid memory from exclude_range[last].end -
		 * memory_max
		 */
		if (i == nr_exclude_ranges - 1) {
			if (exclude_range[i].end < memory_max) {
				memory_range[j].start = exclude_range[i].end + 1;
				memory_range[j].end = memory_max;
				memory_range[j].type = RANGE_RAM;
				j++;
				if (j >= max_memory_ranges)
					realloc_memory_ranges();
				/* Limit the end to rma_top */
				if (memory_range[j-1].start >= rma_top) {
					j--;
					break;
				}
				if ((memory_range[j-1].start < rma_top) &&
				(memory_range[j-1].end >= rma_top)) {
					memory_range[j-1].end = rma_top;
					break;
				}
				continue;
			}
		} /* i == nr_exclude_ranges - 1 */
		/* contiguous exclude ranges - skip */
		if (exclude_range[i+1].start == exclude_range[i].end + 1)
			continue;
		memory_range[j].start = exclude_range[i].end + 1;
		memory_range[j].end = exclude_range[i+1].start - 1;
		memory_range[j].type = RANGE_RAM;
		j++;
		if (j >= max_memory_ranges)
			realloc_memory_ranges();
		/* Limit range to rma_top */
		if (memory_range[j-1].start >= rma_top) {
			j--;
			break;
		}
		if ((memory_range[j-1].start < rma_top) &&
			(memory_range[j-1].end >= rma_top)) {
			memory_range[j-1].end = rma_top;
			break;
		}
	}
	nr_memory_ranges = j;

	int k;
	for (k = 0; k < j; k++)
		dbgprintf("setup_memory_ranges memory_range[%d] "
			"start:%llx, end:%llx\n", k, memory_range[k].start,
			memory_range[k].end);
	return 0;

out:
	cleanup_memory_ranges();
	return -1;
}

/* Return a list of valid memory ranges */
int get_memory_ranges(struct memory_range **range, int *ranges,
			unsigned long kexec_flags)
{
        /* allocate memory_range dynamically */
        max_memory_ranges = 1;

	if (alloc_memory_ranges())
		return -1;
	if (setup_memory_ranges(kexec_flags))
		return -1;

	/*
	 * copy the memory here, another realloc_memory_ranges might
	 * corrupt the old memory
	 */
	*range = calloc(sizeof(struct memory_range), nr_memory_ranges);
	if (*range == NULL)
		return -1;
	memmove(*range, memory_range,
		sizeof(struct memory_range) * nr_memory_ranges);

	*ranges = nr_memory_ranges;
	dbgprintf("get memory ranges:%d\n", nr_memory_ranges);
	return 0;
}

struct file_type file_type[] = {
	{ "elf-ppc64", elf_ppc64_probe, elf_ppc64_load, elf_ppc64_usage },
};
int file_types = sizeof(file_type) / sizeof(file_type[0]);

void arch_usage(void)
{
	printf("     --elf64-core-headers Prepare core headers in ELF64 format\n");
	printf("     --dt-no-old-root Do not reuse old kernel root= param.\n"
	       "                      while creating flatten device tree.\n");
}

struct arch_options_t arch_options = {
	.core_header_type = CORE_TYPE_ELF64,
};

int arch_process_options(int argc, char **argv)
{
	/* We look for all options so getopt_long doesn't start reordering
	 * argv[] before file_type[n].load() gets a look in.
	 */
	static const struct option options[] = {
		KEXEC_ALL_OPTIONS
		{ 0, 0, NULL, 0 },
	};
	static const char short_options[] = KEXEC_ALL_OPT_STR;
	int opt;

	opterr = 0; /* Don't complain about unrecognized options here */
	while((opt = getopt_long(argc, argv, short_options, options, 0)) != -1) {
		switch(opt) {
		default:
			break;
		case OPT_ELF64_CORE:
			arch_options.core_header_type = CORE_TYPE_ELF64;
			break;
		case OPT_DT_NO_OLD_ROOT:
			dt_no_old_root = 1;
			break;
		}
	}
	/* Reset getopt for the next pass; called in other source modules */
	opterr = 1;
	optind = 1;
	return 0;
}

const struct arch_map_entry arches[] = {
	/* We are running a 32-bit kexec-tools on 64-bit ppc64.
	 * So pass KEXEC_ARCH_PPC64 here
	 */
	{ "ppc64", KEXEC_ARCH_PPC64 },
	{ "ppc64le", KEXEC_ARCH_PPC64 },
	{ NULL, 0 },
};

int arch_compat_trampoline(struct kexec_info *UNUSED(info))
{
	return 0;
}

void arch_update_purgatory(struct kexec_info *UNUSED(info))
{
}

int arch_do_exclude_segment(struct kexec_info *info, struct kexec_segment *segment)
{
	if (info->elfcorehdr == (unsigned long) segment->mem)
		return 1;

	if (segment->buf && fdt_magic(segment->buf) == FDT_MAGIC)
		return 1;

	return 0;
}
