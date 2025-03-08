/*
 * kexec-ppc.c - kexec for the PowerPC
 * Copyright (C) 2004, 2005 Albert Herranz
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "../../kexec.h"
#include "../../kexec-syscall.h"
#include "kexec-ppc.h"
#include "crashdump-powerpc.h"
#include <arch/options.h>

#include "config.h"

unsigned long dt_address_cells = 0, dt_size_cells = 0;
uint64_t rmo_top;
uint64_t memory_limit;
unsigned long long crash_base = 0, crash_size = 0;
unsigned long long initrd_base = 0, initrd_size = 0;
unsigned long long ramdisk_base = 0, ramdisk_size = 0;
unsigned int rtas_base, rtas_size;
int max_memory_ranges;
const char *ramdisk;

/*
 * Reads the #address-cells and #size-cells on this platform.
 * This is used to parse the memory/reg info from the device-tree
 */
int init_memory_region_info()
{
	size_t res = 0;
	int fd;
	char *file;

	file = "/proc/device-tree/#address-cells";
	fd = open(file, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Unable to open %s\n", file);
		return -1;
	}

	res = read(fd, &dt_address_cells, sizeof(dt_address_cells));
	if (res != sizeof(dt_address_cells)) {
		fprintf(stderr, "Error reading %s\n", file);
		return -1;
	}
	close(fd);

	file = "/proc/device-tree/#size-cells";
	fd = open(file, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Unable to open %s\n", file);
		return -1;
	}

	res = read(fd, &dt_size_cells, sizeof(dt_size_cells));
	if (res != sizeof(dt_size_cells)) {
		fprintf(stderr, "Error reading %s\n", file);
		return -1;
	}
	close(fd);

	/* Convert the sizes into bytes */
	dt_size_cells *= sizeof(unsigned long);
	dt_address_cells *= sizeof(unsigned long);

	return 0;
}

#define MAXBYTES 128
/*
 * Reads the memory region info from the device-tree node pointed
 * by @fd and fills the *start, *end with the boundaries of the region
 */
int read_memory_region_limits(int fd, unsigned long long *start,
				unsigned long long *end)
{
	char buf[MAXBYTES];
	unsigned long *p;
	unsigned long nbytes = dt_address_cells + dt_size_cells;

	if (lseek(fd, 0, SEEK_SET) == -1) {
		fprintf(stderr, "Error in file seek\n");
		return -1;
	}
	if (read(fd, buf, nbytes) != nbytes) {
		fprintf(stderr, "Error reading the memory region info\n");
		return -1;
	}

	p = (unsigned long*)buf;
	if (dt_address_cells == sizeof(unsigned long)) {
		*start = p[0];
		p++;
	} else if (dt_address_cells == sizeof(unsigned long long)) {
		*start = ((unsigned long long *)p)[0];
		p = (unsigned long long *)p + 1;
	} else {
		fprintf(stderr, "Unsupported value for #address-cells : %ld\n",
					dt_address_cells);
		return -1;
	}

	if (dt_size_cells == sizeof(unsigned long))
		*end = *start + p[0];
	else if (dt_size_cells == sizeof(unsigned long long))
		*end = *start + ((unsigned long long *)p)[0];
	else {
		fprintf(stderr, "Unsupported value for #size-cells : %ld\n",
					dt_size_cells);
		return -1;
	}

	return 0;
}

void arch_reuse_initrd(void)
{
	reuse_initrd = 1;
}

#ifdef WITH_GAMECUBE
#define MAX_MEMORY_RANGES  64
static struct memory_range memory_range[MAX_MEMORY_RANGES];

static int get_memory_ranges_gc(struct memory_range **range, int *ranges,
					unsigned long UNUSED(kexec_flags))
{
	int memory_ranges = 0;

	/* RAM - lowmem used by DOLs - framebuffer */
	memory_range[memory_ranges].start = 0x00003000;
	memory_range[memory_ranges].end = 0x0174bfff;
	memory_range[memory_ranges].type = RANGE_RAM;
	memory_ranges++;
	*range = memory_range;
	*ranges = memory_ranges;
	return 0;
}
#else
static int use_new_dtb;
static int nr_memory_ranges, nr_exclude_ranges;
static struct memory_range *exclude_range;
static struct memory_range *memory_range;
static struct memory_range *base_memory_range;
static uint64_t memory_max;

/*
 * Count the memory nodes under /proc/device-tree and populate the
 * max_memory_ranges variable. This variable replaces MAX_MEMORY_RANGES
 * macro used earlier.
 */
static int count_memory_ranges(void)
{
	char device_tree[256] = "/proc/device-tree/";
	struct dirent *dentry;
	DIR *dir;

	if ((dir = opendir(device_tree)) == NULL) {
		perror(device_tree);
		return -1;
	}

	while ((dentry = readdir(dir)) != NULL) {
		if (strncmp(dentry->d_name, "memory@", 7) &&
				strcmp(dentry->d_name, "memory"))
			continue;
		max_memory_ranges++;
	}

	/* need to add extra region for retained initrd */
	if (use_new_dtb) {
		max_memory_ranges++;
	}

	closedir(dir);
	return 0;

}

 static void cleanup_memory_ranges(void)
 {
	 free(memory_range);
	 free(base_memory_range);
	 free(exclude_range);
 }

/*
 * Allocate memory for various data structures used to hold
 * values of different memory ranges
 */
static int alloc_memory_ranges(void)
{
	int memory_range_len;

	memory_range_len = sizeof(struct memory_range) * max_memory_ranges;

	memory_range = malloc(memory_range_len);
	if (!memory_range)
		return -1;

	base_memory_range = malloc(memory_range_len);
	if (!base_memory_range)
		goto err1;

	exclude_range = malloc(memory_range_len);
	if (!exclude_range)
		goto err1;

	memset(memory_range, 0, memory_range_len);
	memset(base_memory_range, 0, memory_range_len);
	memset(exclude_range, 0, memory_range_len);
	return 0;

err1:
	fprintf(stderr, "memory range structure allocation failure\n");
	cleanup_memory_ranges();
	return -1;
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

static int realloc_memory_ranges(void)
{
	size_t memory_range_len;

	max_memory_ranges++;
	memory_range_len = sizeof(struct memory_range) * max_memory_ranges;

	memory_range = (struct memory_range *) realloc(memory_range,
						       memory_range_len);
	if (!memory_range)
		goto err;

	base_memory_range = (struct memory_range *) realloc(base_memory_range,
			memory_range_len);
	if (!base_memory_range)
		goto err;

	exclude_range = (struct memory_range *) realloc(exclude_range,
			memory_range_len);
	if (!exclude_range)
		goto err;

	usablemem_rgns.ranges = (struct memory_range *)
				realloc(usablemem_rgns.ranges,
						memory_range_len);
	if (!(usablemem_rgns.ranges))
		goto err;

	return 0;

err:
	fprintf(stderr, "memory range structure re-allocation failure\n");
	return -1;
}

/* Get base memory ranges */
static int get_base_ranges(void)
{
	int local_memory_ranges = 0;
	char device_tree[256] = "/proc/device-tree/";
	char fname[256];
	char buf[MAXBYTES];
	DIR *dir, *dmem;
	struct dirent *dentry, *mentry;
	int n, fd;

	if ((dir = opendir(device_tree)) == NULL) {
		perror(device_tree);
		return -1;
	}
	while ((dentry = readdir(dir)) != NULL) {
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
			unsigned long long start, end;

			if (strcmp(mentry->d_name, "reg"))
				continue;
			strcat(fname, "/reg");
			if ((fd = open(fname, O_RDONLY)) < 0) {
				perror(fname);
				closedir(dmem);
				closedir(dir);
				return -1;
			}
			if (read_memory_region_limits(fd, &start, &end) != 0) {
				close(fd);
				closedir(dmem);
				closedir(dir);
				return -1;
			}
			if (local_memory_ranges >= max_memory_ranges) {
				if (realloc_memory_ranges() < 0){
					close(fd);
					break;
				}
			}

			base_memory_range[local_memory_ranges].start = start;
			base_memory_range[local_memory_ranges].end  = end;
			base_memory_range[local_memory_ranges].type = RANGE_RAM;
			local_memory_ranges++;
			dbgprintf("%016llx-%016llx : %x\n",
					base_memory_range[local_memory_ranges-1].start,
					base_memory_range[local_memory_ranges-1].end,
					base_memory_range[local_memory_ranges-1].type);
			close(fd);
		}
		closedir(dmem);
	}
	closedir(dir);
	nr_memory_ranges = local_memory_ranges;
	sort_base_ranges();
	memory_max = base_memory_range[nr_memory_ranges - 1].end;

	dbgprintf("get base memory ranges:%d\n", nr_memory_ranges);

	return 0;
}

static int read_kernel_memory_limit(char *fname, char *buf)
{
	FILE *file;
	int n;

	if (!fname || !buf)
		return -1;

	file = fopen(fname, "r");
	if (file == NULL) {
		if (errno != ENOENT) {
			perror(fname);
			return -1;
		}
		errno = 0;
		/*
		 * fall through. On older kernel this file
		 * is not present. Hence return success.
		 */
	} else {
		/* Memory limit property is of u64 type. */
		if ((n = fread(&memory_limit, 1, sizeof(uint64_t), file)) < 0) {
			perror(fname);
			goto err_out;
		}
		if (n != sizeof(uint64_t)) {
			fprintf(stderr, "%s node has invalid size: %d\n",
						fname, n);
			goto err_out;
		}
		fclose(file);
	}
	return 0;
err_out:
	fclose(file);
	return -1;
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
	uint64_t rmo_base;
	unsigned long long tce_base;
	unsigned int tce_size;
	unsigned long long htab_base, htab_size;
	unsigned long long kernel_end;
	unsigned long long initrd_start, initrd_end;
	char buf[MAXBYTES];
	char device_tree[256] = "/proc/device-tree/";
	char fname[256];
	DIR *dir, *cdir;
	FILE *file;
	struct dirent *dentry;
	int n, i = 0;

	if ((dir = opendir(device_tree)) == NULL) {
		perror(device_tree);
		return -1;
	}

	while ((dentry = readdir(dir)) != NULL) {
		if (strncmp(dentry->d_name, "chosen", 6) &&
				strncmp(dentry->d_name, "memory@", 7) &&
				strncmp(dentry->d_name, "memory", 6) &&
				strncmp(dentry->d_name, "pci@", 4) &&
				strncmp(dentry->d_name, "rtas", 4))
			continue;
		strcpy(fname, device_tree);
		strcat(fname, dentry->d_name);
		if ((cdir = opendir(fname)) == NULL) {
			perror(fname);
			goto error_opendir;
		}

		if (strncmp(dentry->d_name, "chosen", 6) == 0) {
			/* only reserve kernel region if we are doing a crash kernel */
			if (kexec_flags & KEXEC_ON_CRASH) {
				strcat(fname, "/linux,kernel-end");
				file = fopen(fname, "r");
				if (!file) {
					perror(fname);
					goto error_opencdir;
				}
				if ((n = fread(buf, 1, MAXBYTES, file)) < 0) {
					perror(fname);
					goto error_openfile;
				}
				if (n == sizeof(uint32_t)) {
					kernel_end = ((uint32_t *)buf)[0];
				} else if (n == sizeof(uint64_t)) {
					kernel_end = ((uint64_t *)buf)[0];
				} else {
					fprintf(stderr, "%s node has invalid size: %d\n", fname, n);
					goto error_openfile;
				}
				fclose(file);

				/* Add kernel memory to exclude_range */
				exclude_range[i].start = 0x0UL;
				exclude_range[i].end = kernel_end;
				i++;
				if (i >= max_memory_ranges)
					realloc_memory_ranges();
				memset(fname, 0, sizeof(fname));
				sprintf(fname, "%s%s%s",
					device_tree, dentry->d_name,
					"/linux,crashkernel-base");
				file = fopen(fname, "r");
				if (!file) {
					perror(fname);
					goto error_opencdir;
				}
				if ((n = fread(buf, 1, MAXBYTES, file)) < 0) {
					perror(fname);
					goto error_openfile;
				}
				if (n == sizeof(uint32_t)) {
					crash_base = ((uint32_t *)buf)[0];
				} else if (n == sizeof(uint64_t)) {
					crash_base = ((uint64_t *)buf)[0];
				} else {
					fprintf(stderr, "%s node has invalid size: %d\n", fname, n);
					goto error_openfile;
				}
				fclose(file);

				memset(fname, 0, sizeof(fname));
				sprintf(fname, "%s%s%s",
					device_tree, dentry->d_name,
					"/linux,crashkernel-size");
				file = fopen(fname, "r");
				if (!file) {
					perror(fname);
					goto error_opencdir;
				}
				if ((n = fread(buf, 1, MAXBYTES, file)) < 0) {
					perror(fname);
					goto error_openfile;
				}
				if (n == sizeof(uint32_t)) {
					crash_size = ((uint32_t *)buf)[0];
				} else if (n == sizeof(uint64_t)) {
					crash_size = ((uint64_t *)buf)[0];
				} else {
					fprintf(stderr, "%s node has invalid size: %d\n", fname, n);
					goto error_openfile;
				}
				fclose(file);

				if (crash_base > mem_min)
					mem_min = crash_base;
				if (crash_base + crash_size < mem_max)
					mem_max = crash_base + crash_size;

#ifndef CONFIG_BOOKE
				add_usable_mem_rgns(0, crash_base + crash_size);
				/* Reserve the region (KDUMP_BACKUP_LIMIT,crash_base) */
				reserve(KDUMP_BACKUP_LIMIT,
						crash_base-KDUMP_BACKUP_LIMIT);
#else
				add_usable_mem_rgns(crash_base, crash_size);
#endif
			}
			/*
			 * Read the first kernel's memory limit.
			 * If the first kernel is booted with mem= option then
			 * it would export "linux,memory-limit" file
			 * reflecting value for the same.
			 */
			memset(fname, 0, sizeof(fname));
			snprintf(fname, sizeof(fname), "%s%s%s", device_tree,
				dentry->d_name, "/linux,memory-limit");
			if (read_kernel_memory_limit(fname, buf) < 0)
				goto error_opencdir;

			/* reserve the initrd_start and end locations. */
			memset(fname, 0, sizeof(fname));
			sprintf(fname, "%s%s%s",
				device_tree, dentry->d_name,
				"/linux,initrd-start");
			file = fopen(fname, "r");
			if (!file) {
				errno = 0;
				initrd_start = 0;
			} else {
				if ((n = fread(buf, 1, MAXBYTES, file)) < 0) {
					perror(fname);
					goto error_openfile;
				}
				if (n == sizeof(uint32_t)) {
					initrd_start = ((uint32_t *)buf)[0];
				} else if (n == sizeof(uint64_t)) {
					initrd_start = ((uint64_t *)buf)[0];
				} else {
					fprintf(stderr, "%s node has invalid size: %d\n", fname, n);
					goto error_openfile;
				}
				fclose(file);
			}

			memset(fname, 0, sizeof(fname));
			sprintf(fname, "%s%s%s",
				device_tree, dentry->d_name,
				"/linux,initrd-end");
			file = fopen(fname, "r");
			if (!file) {
				errno = 0;
				initrd_end = 0;
			} else {
				if ((n = fread(buf, 1, MAXBYTES, file)) < 0) {
					perror(fname);
					goto error_openfile;
				}
				if (n == sizeof(uint32_t)) {
					initrd_end = ((uint32_t *)buf)[0];
				} else if (n == sizeof(uint64_t)) {
					initrd_end = ((uint64_t *)buf)[0];
				} else {
					fprintf(stderr, "%s node has invalid size: %d\n", fname, n);
					goto error_openfile;
				}
				fclose(file);
			}

			if ((initrd_end - initrd_start) != 0 ) {
				initrd_base = initrd_start;
				initrd_size = initrd_end - initrd_start;
			}

			if (reuse_initrd) {
				/* Add initrd address to exclude_range */
				exclude_range[i].start = initrd_start;
				exclude_range[i].end = initrd_end;
				i++;
				if (i >= max_memory_ranges)
					realloc_memory_ranges();
			}

			/* HTAB */
			memset(fname, 0, sizeof(fname));
			sprintf(fname, "%s%s%s",
				device_tree, dentry->d_name,
				"/linux,htab-base");
			file = fopen(fname, "r");
			if (!file) {
				closedir(cdir);
				if (errno == ENOENT) {
					/* Non LPAR */
					errno = 0;
					continue;
				}
				perror(fname);
				goto error_opendir;
			}
			if (fread(&htab_base, sizeof(unsigned long), 1, file)
					!= 1) {
				perror(fname);
				goto error_openfile;
			}
			memset(fname, 0, sizeof(fname));
			sprintf(fname, "%s%s%s",
				device_tree, dentry->d_name,
				"/linux,htab-size");
			file = fopen(fname, "r");
			if (!file) {
				perror(fname);
				goto error_opencdir;
			}
			if (fread(&htab_size, sizeof(unsigned long), 1, file)
					!= 1) {
				perror(fname);
				goto error_openfile;
			}
			/* Add htab address to exclude_range - NON-LPAR only */
			exclude_range[i].start = htab_base;
			exclude_range[i].end = htab_base + htab_size;
			i++;
			if (i >= max_memory_ranges)
				realloc_memory_ranges();

		} /* chosen */
		if (strncmp(dentry->d_name, "rtas", 4) == 0) {
			strcat(fname, "/linux,rtas-base");
			if ((file = fopen(fname, "r")) == NULL) {
				perror(fname);
				goto error_opencdir;
			}
			if (fread(&rtas_base, sizeof(unsigned int), 1, file)
					!= 1) {
				perror(fname);
				goto error_openfile;
			}
			memset(fname, 0, sizeof(fname));
			sprintf(fname, "%s%s%s",
				device_tree, dentry->d_name,
				"/linux,rtas-size");
			if ((file = fopen(fname, "r")) == NULL) {
				perror(fname);
				goto error_opencdir;
			}
			if (fread(&rtas_size, sizeof(unsigned int), 1, file)
					!= 1) {
				perror(fname);
				goto error_openfile;
			}
			closedir(cdir);
			/* Add rtas to exclude_range */
			exclude_range[i].start = rtas_base;
			exclude_range[i].end = rtas_base + rtas_size;
			i++;
			if (kexec_flags & KEXEC_ON_CRASH)
				add_usable_mem_rgns(rtas_base, rtas_size);
		} /* rtas */

		if (!strncmp(dentry->d_name, "memory@", 7) ||
				!strcmp(dentry->d_name, "memory")) {
			int fd;
			strcat(fname, "/reg");
			if ((fd = open(fname, O_RDONLY)) < 0) {
				perror(fname);
				goto error_opencdir;
			}
			if (read_memory_region_limits(fd, &rmo_base, &rmo_top) != 0)
				goto error_openfile;

			if (rmo_top > 0x30000000UL)
				rmo_top = 0x30000000UL;

			close(fd);
			closedir(cdir);
		} /* memory */

		if (strncmp(dentry->d_name, "pci@", 4) == 0) {
			strcat(fname, "/linux,tce-base");
			file = fopen(fname, "r");
			if (!file) {
				closedir(cdir);
				if (errno == ENOENT) {
					/* Non LPAR */
					errno = 0;
					continue;
				}
				perror(fname);
				goto error_opendir;
			}
			if (fread(&tce_base, sizeof(unsigned long), 1, file)
					!= 1) {
				perror(fname);
				goto error_openfile;
			}
			memset(fname, 0, sizeof(fname));
			sprintf(fname, "%s%s%s",
				device_tree, dentry->d_name,
				"/linux,tce-size");
			file = fopen(fname, "r");
			if (!file) {
				perror(fname);
				goto error_opencdir;
			}
			if (fread(&tce_size, sizeof(unsigned int), 1, file)
					!= 1) {
				perror(fname);
				goto error_openfile;
			}
			/* Add tce to exclude_range - NON-LPAR only */
			exclude_range[i].start = tce_base;
			exclude_range[i].end = tce_base + tce_size;
			i++;
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
static int setup_memory_ranges(unsigned long kexec_flags)
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
				/* Limit the end to rmo_top */
				if (memory_range[j-1].start >= rmo_top) {
					j--;
					break;
				}
				if ((memory_range[j-1].start < rmo_top) &&
						(memory_range[j-1].end >= rmo_top)) {
					memory_range[j-1].end = rmo_top;
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
		/* Limit range to rmo_top */
		if (memory_range[j-1].start >= rmo_top) {
			j--;
			break;
		}
		if ((memory_range[j-1].start < rmo_top) &&
				(memory_range[j-1].end >= rmo_top)) {
			memory_range[j-1].end = rmo_top;
			break;
		}
	}

	/* fixup in case we have no exclude regions */
	if (!j) {
		memory_range[0].start = base_memory_range[0].start;
		memory_range[0].end = rmo_top;
		memory_range[0].type = RANGE_RAM;
		nr_memory_ranges = 1;
	} else
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
int get_memory_ranges_dt(struct memory_range **range, int *ranges,
		unsigned long kexec_flags)
{
	if (count_memory_ranges())
		return -1;
	if (alloc_memory_ranges())
		return -1;
	if (setup_memory_ranges(kexec_flags))
		return -1;

	*range = memory_range;
	*ranges = nr_memory_ranges;
	return 0;
}
#endif

/* Return a sorted list of memory ranges. */
int get_memory_ranges(struct memory_range **range, int *ranges,
					unsigned long kexec_flags)
{
	int res = 0;

	res = init_memory_region_info();
	if (res != 0)
		return res;
#ifdef WITH_GAMECUBE
	return get_memory_ranges_gc(range, ranges, kexec_flags);
#else
	return get_memory_ranges_dt(range, ranges, kexec_flags);
#endif
}

struct file_type file_type[] = {
	{"elf-ppc", elf_ppc_probe, elf_ppc_load, elf_ppc_usage},
	{"dol-ppc", dol_ppc_probe, dol_ppc_load, dol_ppc_usage},
	{"uImage-ppc", uImage_ppc_probe, uImage_ppc_load, uImage_ppc_usage },
};
int file_types = sizeof(file_type) / sizeof(file_type[0]);

void arch_usage(void)
{
}

int arch_process_options(int argc, char **argv)
{
	return 0;
}

const struct arch_map_entry arches[] = {
	/* For compatibility with older patches
	 * use KEXEC_ARCH_DEFAULT instead of KEXEC_ARCH_PPC here.
	 */
	{ "ppc", KEXEC_ARCH_DEFAULT },
	{ NULL, 0 },
};

int arch_compat_trampoline(struct kexec_info *UNUSED(info))
{
	return 0;
}

void arch_update_purgatory(struct kexec_info *UNUSED(info))
{
}

int arch_do_exclude_segment(struct kexec_info *UNUSED(info), struct kexec_segment *UNUSED(segment))
{
	return 0;
}
