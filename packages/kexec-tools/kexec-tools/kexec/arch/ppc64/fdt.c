/*
 * ppc64 fdt fixups
 *
 * Copyright 2015 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation (version 2 of the License).
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arch/fdt.h>
#include <libfdt.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>

#include "../../kexec.h"
#include "../../kexec-syscall.h"

/*
 * Let the kernel know it booted from kexec, as some things (e.g.
 * secondary CPU release) may work differently.
 */
static int fixup_kexec_prop(void *fdt)
{
	int err, nodeoffset;

	nodeoffset = fdt_subnode_offset(fdt, 0, "chosen");
	if (nodeoffset < 0)
		nodeoffset = fdt_add_subnode(fdt, 0, "chosen");
	if (nodeoffset < 0) {
		printf("%s: add /chosen %s\n", __func__,
		       fdt_strerror(nodeoffset));
		return -1;
	}

	err = fdt_setprop(fdt, nodeoffset, "linux,booted-from-kexec",
			  NULL, 0);
	if (err < 0) {
		printf("%s: couldn't write linux,booted-from-kexec: %s\n",
		       __func__, fdt_strerror(err));
		return -1;
	}

	return 0;
}

static inline bool is_dot_dir(char * d_path)
{
	return d_path[0] == '.';
}

/*
 * get_cpu_node_size - Returns size of files including file name size under
 *                     the given @cpu_node_path.
 */
static int get_cpu_node_size(char *cpu_node_path)
{
	DIR *d;
	struct dirent *de;
	struct stat statbuf;
	int cpu_node_size = 0;
	char cpu_prop_path[2 * PATH_MAX];

	d = opendir(cpu_node_path);
	if (!d)
		return 0;

	while ((de = readdir(d)) != NULL) {
		if (de->d_type != DT_REG)
			continue;

		memset(cpu_prop_path, '\0', PATH_MAX);
		snprintf(cpu_prop_path, 2 * PATH_MAX, "%s/%s", cpu_node_path,
			 de->d_name);

		if (stat(cpu_prop_path, &statbuf))
			continue;

		cpu_node_size += statbuf.st_size;
		cpu_node_size += strlen(de->d_name);
	}

	return cpu_node_size;
}

/*
 * is_cpu_node - Checks if the node specified by the given @path
 *               represents a CPU node.
 *
 * Returns true if the @path has a "device_type" file containing "cpu";
 * otherwise, returns false.
 */
static bool is_cpu_node(char *path)
{
	FILE *file;
	bool ret = false;
	char device_type[4];

	file = fopen(path, "r");
	if (!file)
		return false;

	memset(device_type, '\0', 4);
	if (fread(device_type, 1, 3, file) < 3)
		goto out;

	if (strcmp(device_type, "cpu"))
		goto out;

	ret = true;
out:
	fclose(file);
	return ret;
}

static int get_threads_per_cpu(char *path)
{
	struct stat statbuf;
	if (stat(path, &statbuf))
		return 0;

	return statbuf.st_size / 4;
}

/**
 * get_present_cpus - finds the present CPUs in the system
 *
 * This function opens the file `/sys/devices/system/cpu/present` to read
 * the range of present CPUs. It parses the range and calculates the
 * total number of present CPUs in the system.
 *
 * Returns total number of present CPUs on success, -1 on failure.
 */
static int get_present_cpus()
{
	char *range;
	char buf[1024];
	int start, end;
	int cpu_count = 0;
	FILE *file = fopen("/sys/devices/system/cpu/present", "r");

	if (!file)
		return -1;

	if (!fgets(buf, sizeof(buf), file))
		return -1;

	fclose(file);

	range = strtok(buf, ",");
	while (range != NULL) {
		if (sscanf(range, "%d-%d", &start, &end) == 2) {
			for (int i = start; i <= end; i++)
				cpu_count++;
		} else if (sscanf(range, "%d", &start) == 1) {
			cpu_count++;
		} else {
			return -1;
		}
		range = strtok(NULL, ",");
	}

	return cpu_count;
}

/*
 * get_cpu_info - Finds the following CPU attributes:
 *
 * threads_per_cpu: Number of threads per CPU, based on the device tree entry
 *                  /proc/device-tree/cpus/<cpu_node>/ibm,ppc-interrupt-server#s.
 * cpu_node_size: Size of files including file name size under a CPU node.
 *
 * Returns 0 on success, else -1.
 */
static int get_cpu_info(int *_present_cpus, int *_threads_per_cpu, int *_cpu_node_size)
{
	DIR *d;
	struct dirent *de;
	char path[PATH_MAX];
	int present_cpus = 0, threads_per_cpu = 0, cpu_node_size = 0;
	char *cpus_node_path = "/proc/device-tree/cpus";

	present_cpus = get_present_cpus();
	if (present_cpus < 0)
		return -1;

	d = opendir(cpus_node_path);
	if (!d)
		return -1;

	while ((de = readdir(d)) != NULL) {
		if ((de->d_type != DT_DIR) || is_dot_dir(de->d_name))
			continue;

		memset(path, '\0', PATH_MAX);
		snprintf(path, PATH_MAX, "%s/%s/%s", cpus_node_path,
			 de->d_name, "device_type");

		/* Skip nodes with device_type != "cpu" */
		if (!is_cpu_node(path))
			continue;

		/*
		 * Found the first node under /proc/device-tree/cpus with
		 * device_type == "cpu"
		 */
		memset(path, '\0', PATH_MAX);
		snprintf(path, PATH_MAX, "%s/%s", cpus_node_path, de->d_name);
		cpu_node_size = get_cpu_node_size(path);

		memset(path, '\0', PATH_MAX);
		snprintf(path, PATH_MAX, "%s/%s/%s", cpus_node_path,
		de->d_name, "ibm,ppc-interrupt-server#s");
		threads_per_cpu = get_threads_per_cpu(path);
		break;
	}

	closedir(d);

	if (!(threads_per_cpu && cpu_node_size))
		return -1;

	*_present_cpus = present_cpus;
	*_cpu_node_size = cpu_node_size;
	*_threads_per_cpu = threads_per_cpu;

	dbgprintf("present_cpus: %d, threads_per_cpu: %d, cpu_node_size: %d\n",
		  present_cpus, threads_per_cpu, cpu_node_size);

	return 0;
}

/*
 * kdump_fdt_extra_size - Calculates the extra size needed for the Flattened
 *                        Device Tree (FDT) based on the possible and present
 *                        CPUs in the system.
 */
static unsigned int kdump_fdt_extra_size(void)
{
	int cpus_in_system;
	unsigned int extra_size = 0;
	int present_cpus = 0, threads_per_cpu = 0, cpu_node_size = 0;
	int possible_cpus;

	/* ALL possible CPUs are present in FDT so no extra size required */
	if (sysconf(_SC_NPROCESSORS_ONLN) == sysconf(_SC_NPROCESSORS_CONF))
		return 0;

	if (get_cpu_info(&present_cpus, &threads_per_cpu, &cpu_node_size)) {
		die("Failed to get cpu info\n");
	}

	cpus_in_system = present_cpus / threads_per_cpu;
	possible_cpus = sysconf(_SC_NPROCESSORS_CONF) / threads_per_cpu;
	dbgprintf("cpus_in_system: %d, possible_cpus: %d\n", cpus_in_system,
		  possible_cpus);

	if (cpus_in_system > possible_cpus)
		die("Possible CPU nodes can't be less than active CPU nodes\n");

	extra_size = (possible_cpus - cpus_in_system) * cpu_node_size;
	dbgprintf("kdump fdt extra size: %u\n", extra_size);

	return extra_size;
}

/*
 * For now, assume that the added content fits in the file.
 * This should be the case when flattening from /proc/device-tree,
 * and when passing in a dtb, dtc can be told to add padding.
 */
int fixup_dt(char **fdt, off_t *size, unsigned long kexec_flags)
{
	int ret;

	*size += 4096;

	/* To support --hotplug option for the kexec_load syscall, consider
	 * adding extra buffer to FDT so that the kernel can add CPU nodes
	 * of hot-added CPUs.
	 */
	if (do_hotplug && (kexec_flags & KEXEC_ON_CRASH))
		*size += kdump_fdt_extra_size();

	*fdt = realloc(*fdt, *size);
	if (!*fdt) {
		fprintf(stderr, "%s: out of memory\n", __func__);
		return -1;
	}

	ret = fdt_open_into(*fdt, *fdt, *size);
	if (ret < 0) {
		fprintf(stderr, "%s: fdt_open_into: %s\n", __func__,
			fdt_strerror(ret));
		return -1;
	}

	ret = fixup_kexec_prop(*fdt);
	if (ret < 0)
		return ret;

	return 0;
}
