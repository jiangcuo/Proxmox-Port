/*
 * kexec: Linux boots Linux
 *
 * Copyright (C) 2003-2005  Eric Biederman (ebiederm@xmission.com)
 *
 * Modified (2007-05-15) by Francesco Chiechi to rudely handle mips platform
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
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef HAVE_MEMFD_CREATE
#include <linux/memfd.h>
#include <sys/syscall.h>
#endif
#include <sys/reboot.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef _O_BINARY
#define _O_BINARY 0
#endif
#include <getopt.h>
#include <ctype.h>

#include "config.h"

#include <sha256.h>
#include "kexec.h"
#include "kexec-syscall.h"
#include "kexec-elf.h"
#include "kexec-xen.h"
#include "kexec-sha256.h"
#include "kexec-zlib.h"
#include "kexec-lzma.h"
#include <arch/options.h>

#define KEXEC_LOADED_PATH "/sys/kernel/kexec_loaded"
#define KEXEC_CRASH_LOADED_PATH "/sys/kernel/kexec_crash_loaded"

unsigned long long mem_min = 0;
unsigned long long mem_max = ULONG_MAX;
unsigned long elfcorehdrsz = 0;
int do_hotplug = 0;
static unsigned long kexec_flags = 0;
/* Flags for kexec file (fd) based syscall */
static unsigned long kexec_file_flags = 0;
int kexec_debug = 0;

void dbgprint_mem_range(const char *prefix, struct memory_range *mr, int nr_mr)
{
	int i;
	dbgprintf("%s\n", prefix);
	for (i = 0; i < nr_mr; i++) {
		dbgprintf("%016llx-%016llx (%d)\n", mr[i].start,
			  mr[i].end, mr[i].type);
	}
}

void die(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fflush(stdout);
	fflush(stderr);
	exit(1);
}

static char *xstrdup(const char *str)
{
	char *new = strdup(str);
	if (!new)
		die("Cannot strdup \"%s\": %s\n",
			str, strerror(errno));
	return new;
}

void *xmalloc(size_t size)
{
	void *buf;
	if (!size)
		return NULL;
	buf = malloc(size);
	if (!buf) {
		die("Cannot malloc %ld bytes: %s\n",
			size + 0UL, strerror(errno));
	}
	return buf;
}

void *xrealloc(void *ptr, size_t size)
{
	void *buf;
	buf = realloc(ptr, size);
	if (!buf) {
		die("Cannot realloc %ld bytes: %s\n",
			size + 0UL, strerror(errno));
	}
	return buf;
}

int valid_memory_range(struct kexec_info *info,
		       unsigned long sstart, unsigned long send)
{
	int i;
	if (sstart > send) {
		return 0;
	}
	if ((send > mem_max) || (sstart < mem_min)) {
		return 0;
	}
	for (i = 0; i < info->memory_ranges; i++) {
		unsigned long mstart, mend;
		/* Only consider memory ranges */
		if (info->memory_range[i].type != RANGE_RAM)
			continue;
		mstart = info->memory_range[i].start;
		mend = info->memory_range[i].end;
		if (i < info->memory_ranges - 1
		    && mend == info->memory_range[i+1].start
		    && info->memory_range[i+1].type == RANGE_RAM)
			mend = info->memory_range[i+1].end;

		/* Check to see if we are fully contained */
		if ((mstart <= sstart) && (mend >= send)) {
			return 1;
		}
	}
	return 0;
}

static int valid_memory_segment(struct kexec_info *info,
				struct kexec_segment *segment)
{
	unsigned long sstart, send;
	sstart = (unsigned long)segment->mem;
	send   = sstart + segment->memsz - 1;

	return valid_memory_range(info, sstart, send);
}

void print_segments(FILE *f, struct kexec_info *info)
{
	int i;

	fprintf(f, "nr_segments = %d\n", info->nr_segments);
	for (i = 0; i < info->nr_segments; i++) {
		fprintf(f, "segment[%d].buf   = %p\n",	i,
			info->segment[i].buf);
		fprintf(f, "segment[%d].bufsz = 0x%zx\n", i,
			info->segment[i].bufsz);
		fprintf(f, "segment[%d].mem   = %p\n",	i,
			info->segment[i].mem);
		fprintf(f, "segment[%d].memsz = 0x%zx\n", i,
			info->segment[i].memsz);
	}
}

int sort_segments(struct kexec_info *info)
{
	int i, j;
	void *end;

	/* Do a stupid insertion sort... */
	for (i = 0; i < info->nr_segments; i++) {
		int tidx;
		struct kexec_segment temp;
		tidx = i;
		for (j = i +1; j < info->nr_segments; j++) {
			if (info->segment[j].mem < info->segment[tidx].mem) {
				tidx = j;
			}
		}
		if (tidx != i) {
			temp = info->segment[tidx];
			info->segment[tidx] = info->segment[i];
			info->segment[i] = temp;
		}
	}
	/* Now see if any of the segments overlap */
	end = 0;
	for (i = 0; i < info->nr_segments; i++) {
		if (end > info->segment[i].mem) {
			fprintf(stderr, "Overlapping memory segments at %p\n",
				end);
			return -1;
		}
		end = ((char *)info->segment[i].mem) + info->segment[i].memsz;
	}
	return 0;
}

unsigned long locate_hole(struct kexec_info *info,
	unsigned long hole_size, unsigned long hole_align, 
	unsigned long hole_min, unsigned long hole_max, 
	int hole_end)
{
	int i, j;
	struct memory_range *mem_range;
	int max_mem_ranges, mem_ranges;
	unsigned long hole_base;

	if (hole_end == 0) {
		die("Invalid hole end argument of 0 specified to locate_hole");
	}

	/* Set an initial invalid value for the hole base */
	hole_base = ULONG_MAX;

	/* Align everything to at least a page size boundary */
	if (hole_align < (unsigned long)getpagesize()) {
		hole_align = getpagesize();
	}

	/* Compute the free memory ranges */
	max_mem_ranges = info->memory_ranges + info->nr_segments;
	mem_range = xmalloc(max_mem_ranges *sizeof(struct memory_range));
	mem_ranges = 0;
		
	/* Perform a merge on the 2 sorted lists of memory ranges  */
	for (j = 0, i = 0; i < info->memory_ranges; i++) {
		unsigned long long sstart, send;
		unsigned long long mstart, mend;
		mstart = info->memory_range[i].start;
		mend = info->memory_range[i].end;
		if (info->memory_range[i].type != RANGE_RAM)
			continue;
		while ((j < info->nr_segments) &&
		       (((unsigned long)info->segment[j].mem) <= mend)) {
			sstart = (unsigned long)info->segment[j].mem;
			send = sstart + info->segment[j].memsz -1;
			if (mstart < sstart) {
				mem_range[mem_ranges].start = mstart;
				mem_range[mem_ranges].end = sstart -1;
				mem_range[mem_ranges].type = RANGE_RAM;
				mem_ranges++;
			}
			mstart = send +1;
			j++;
		}
		if (mstart < mend) {
			mem_range[mem_ranges].start = mstart;
			mem_range[mem_ranges].end = mend;
			mem_range[mem_ranges].type = RANGE_RAM;
			mem_ranges++;
		}
	}
	/* Now find the end of the last memory_range I can use */
	for (i = 0; i < mem_ranges; i++) {
		unsigned long long start, end, size;
		start = mem_range[i].start;
		end   = mem_range[i].end;
		/* First filter the range start and end values
		 * through the lens of mem_min, mem_max and hole_align.
		 */
		if (start < mem_min) {
			start = mem_min;
		}
		if (start < hole_min) {
			start = hole_min;
		}
		start = _ALIGN(start, hole_align);
		if (end > mem_max) {
			end = mem_max;
		}
		if (end > hole_max) {
			end = hole_max;
		}
		/* Is this still a valid memory range? */
		if ((start >= end) || (start >= mem_max) || (end <= mem_min)) {
			continue;
		}
		/* Is there enough space left so we can use it? */
		size = end - start;
		if (!hole_size || size >= hole_size - 1) {
			if (hole_end > 0) {
				hole_base = start;
				break;
			} else {
				hole_base = _ALIGN_DOWN(end - hole_size + 1,
					hole_align);
			}
		}
	}
	free(mem_range);
	if (hole_base == ULONG_MAX) {
		fprintf(stderr, "Could not find a free area of memory of "
			"0x%lx bytes...\n", hole_size);
		return ULONG_MAX;
	}
	if (hole_size && (hole_base + hole_size - 1)  > hole_max) {
		fprintf(stderr, "Could not find a free area of memory below: "
			"0x%lx...\n", hole_max);
		return ULONG_MAX;
	}
	return hole_base;
}

void add_segment_phys_virt(struct kexec_info *info,
	const void *buf, size_t bufsz,
	unsigned long base, size_t memsz, int phys)
{
	unsigned long last;
	size_t size;
	int pagesize;

	if (bufsz > memsz) {
		bufsz = memsz;
	}
	/* Forget empty segments */
	if (memsz == 0) {
		return;
	}

	/* Round memsz up to a multiple of pagesize */
	pagesize = getpagesize();
	memsz = _ALIGN(memsz, pagesize);

	/* Verify base is pagesize aligned.
	 * Finding a way to cope with this problem
	 * is important but for now error so at least
	 * we are not surprised by the code doing the wrong
	 * thing.
	 */
	if (base & (pagesize -1)) {
		die("Base address: 0x%lx is not page aligned\n", base);
	}

	if (phys)
		base = virt_to_phys(base);

	last = base + memsz -1;
	if (!valid_memory_range(info, base, last)) {
		die("Invalid memory segment %p - %p\n",
			(void *)base, (void *)last);
	}

	size = (info->nr_segments + 1) * sizeof(info->segment[0]);
	info->segment = xrealloc(info->segment, size);
	info->segment[info->nr_segments].buf   = buf;
	info->segment[info->nr_segments].bufsz = bufsz;
	info->segment[info->nr_segments].mem   = (void *)base;
	info->segment[info->nr_segments].memsz = memsz;
	info->nr_segments++;
	if (info->nr_segments > KEXEC_MAX_SEGMENTS) {
		fprintf(stderr, "Warning: kernel segment limit reached. "
			"This will likely fail\n");
	}
}

unsigned long add_buffer_phys_virt(struct kexec_info *info,
	const void *buf, unsigned long bufsz, unsigned long memsz,
	unsigned long buf_align, unsigned long buf_min, unsigned long buf_max,
	int buf_end, int phys)
{
	unsigned long base;
	int result;
	int pagesize;

	result = sort_segments(info);
	if (result < 0) {
		die("sort_segments failed\n");
	}

	/* Round memsz up to a multiple of pagesize */
	pagesize = getpagesize();
	memsz = _ALIGN(memsz, pagesize);

	base = locate_hole(info, memsz, buf_align, buf_min, buf_max, buf_end);
	if (base == ULONG_MAX) {
		die("locate_hole failed\n");
	}
	
	add_segment_phys_virt(info, buf, bufsz, base, memsz, phys);
	return base;
}

unsigned long add_buffer_virt(struct kexec_info *info, const void *buf,
			      unsigned long bufsz, unsigned long memsz,
			      unsigned long buf_align, unsigned long buf_min,
			      unsigned long buf_max, int buf_end)
{
	return add_buffer_phys_virt(info, buf, bufsz, memsz, buf_align,
				    buf_min, buf_max, buf_end, 0);
}

static int find_memory_range(struct kexec_info *info,
			     unsigned long *base, unsigned long *size)
{
	int i;
	unsigned long start, end;

	for (i = 0; i < info->memory_ranges; i++) {
		if (info->memory_range[i].type != RANGE_RAM)
			continue;
		start = info->memory_range[i].start;
		end = info->memory_range[i].end;
		if (end > *base && start < *base + *size) {
			if (start > *base) {
				*size = *base + *size - start;
				*base = start;
			}
			if (end < *base + *size)
				*size = end - *base;
			return 1;
		}
	}
	return 0;
}

static int find_segment_hole(struct kexec_info *info,
			     unsigned long *base, unsigned long *size)
{
	int i;
	unsigned long seg_base, seg_size;

	for (i = 0; i < info->nr_segments; i++) {
		seg_base = (unsigned long)info->segment[i].mem;
		seg_size = info->segment[i].memsz;

		if (seg_base + seg_size <= *base)
			continue;
		else if (seg_base >= *base + *size)
			break;
		else if (*base < seg_base) {
			*size = seg_base - *base;
			break;
		} else if (seg_base + seg_size < *base + *size) {
			*size = *base + *size - (seg_base + seg_size);
			*base = seg_base + seg_size;
		} else {
			*size = 0;
			break;
		}
	}
	return *size;
}

static int add_backup_segments(struct kexec_info *info,
			       unsigned long backup_base,
			       unsigned long backup_size)
{
	unsigned long mem_base, mem_size, bkseg_base, bkseg_size, start, end;
	unsigned long pagesize;

	pagesize = getpagesize();
	while (backup_size) {
		mem_base = backup_base;
		mem_size = backup_size;
		if (!find_memory_range(info, &mem_base, &mem_size))
			break;
		backup_size = backup_base + backup_size - \
			(mem_base + mem_size);
		backup_base = mem_base + mem_size;
		while (mem_size) {
			bkseg_base = mem_base;
			bkseg_size = mem_size;
			if (sort_segments(info) < 0)
				return -1;
			if (!find_segment_hole(info, &bkseg_base, &bkseg_size))
				break;
			start = _ALIGN(bkseg_base, pagesize);
			end = _ALIGN_DOWN(bkseg_base + bkseg_size, pagesize);
			add_segment_phys_virt(info, NULL, 0,
					      start, end-start, 0);
			mem_size = mem_base + mem_size - \
				(bkseg_base + bkseg_size);
			mem_base = bkseg_base + bkseg_size;
		}
	}
	return 0;
}

char *slurp_fd(int fd, const char *filename, off_t size, off_t *nread)
{
	char *buf;
	off_t progress;
	ssize_t result;

	buf = xmalloc(size);
	progress = 0;
	while (progress < size) {
		result = read(fd, buf + progress, size - progress);
		if (result < 0) {
			if ((errno == EINTR) ||	(errno == EAGAIN))
				continue;
			fprintf(stderr, "Read on %s failed: %s\n", filename,
				strerror(errno));
			free(buf);
			close(fd);
			return NULL;
		}
		if (result == 0)
			/* EOF */
			break;
		progress += result;
	}
	result = close(fd);
	if (result < 0)
		die("Close of %s failed: %s\n", filename, strerror(errno));

	if (nread)
		*nread = progress;
	return buf;
}

static char *slurp_file_generic(const char *filename, off_t *r_size,
				int use_mmap)
{
	int fd;
	char *buf;
	off_t size, err, nread;
	ssize_t result;
	struct stat stats;

	if (!filename) {
		*r_size = 0;
		return 0;
	}
	fd = open(filename, O_RDONLY | _O_BINARY);
	if (fd < 0) {
		die("Cannot open `%s': %s\n",
			filename, strerror(errno));
	}
	result = fstat(fd, &stats);
	if (result < 0) {
		die("Cannot stat: %s: %s\n",
			filename, strerror(errno));
	}
	/*
	 * Seek in case the kernel is a character node like /dev/ubi0_0.
	 * This does not work on regular files which live in /proc and
	 * we need this for some /proc/device-tree entries
	 */
	if (S_ISCHR(stats.st_mode)) {

		size = lseek(fd, 0, SEEK_END);
		if (size < 0)
			die("Can not seek file %s: %s\n", filename,
					strerror(errno));

		err = lseek(fd, 0, SEEK_SET);
		if (err < 0)
			die("Can not seek to the begin of file %s: %s\n",
					filename, strerror(errno));
		buf = slurp_fd(fd, filename, size, &nread);
	} else if (S_ISBLK(stats.st_mode)) {
		err = ioctl(fd, BLKGETSIZE64, &size);
		if (err < 0)
			die("Can't retrieve size of block device %s: %s\n",
				filename, strerror(errno));
		buf = slurp_fd(fd, filename, size, &nread);
	} else {
		size = stats.st_size;
		if (use_mmap) {
			buf = mmap(NULL, size, PROT_READ|PROT_WRITE,
				   MAP_PRIVATE, fd, 0);
			nread = size;
		} else {
			buf = slurp_fd(fd, filename, size, &nread);
		}
	}
	if ((use_mmap && (buf == MAP_FAILED)) || (!use_mmap && (buf == NULL)))
		die("Cannot read %s", filename);

	if (nread != size)
		die("Read on %s ended before stat said it should\n", filename);

	*r_size = size;
	close(fd);
	return buf;
}

/*
 * Read file into malloced buffer.
 */
char *slurp_file(const char *filename, off_t *r_size)
{
	return slurp_file_generic(filename, r_size, 0);
}

/*
 * Map "normal" file or read "character device" into malloced buffer.
 * You must not use free, realloc, etc. for the returned buffer.
 */
char *slurp_file_mmap(const char *filename, off_t *r_size)
{
	return slurp_file_generic(filename, r_size, 1);
}

/* This functions reads either specified number of bytes from the file or
   lesser if EOF is met. */

char *slurp_file_len(const char *filename, off_t size, off_t *nread)
{
	int fd;

	if (!filename)
		return 0;
	fd = open(filename, O_RDONLY | _O_BINARY);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n", filename,
				strerror(errno));
		return 0;
	}

	return slurp_fd(fd, filename, size, nread);
}

char *slurp_decompress_file(const char *filename, off_t *r_size)
{
	char *kernel_buf;

	kernel_buf = zlib_decompress_file(filename, r_size);
	if (!kernel_buf) {
		kernel_buf = lzma_decompress_file(filename, r_size);
		if (!kernel_buf)
			return slurp_file(filename, r_size);
	}
	return kernel_buf;
}

#ifndef HAVE_MEMFD_CREATE
static int memfd_create(const char *name, unsigned int flags)
{
	return syscall(SYS_memfd_create, name, flags);
}
#endif

static int copybuf_memfd(const char *kernel_buf, size_t size)
{
	int fd, count;

	fd = memfd_create("kernel", MFD_ALLOW_SEALING);
	if (fd == -1)
		return fd;

	count = write(fd, kernel_buf, size);
	if (count < 0)
		return -1;

	return fd;
}

static void update_purgatory(struct kexec_info *info)
{
	static const uint8_t null_buf[256];
	sha256_context ctx;
	sha256_digest_t digest;
	struct sha256_region region[SHA256_REGIONS];
	int i, j;
	/* Don't do anything if we are not using purgatory */
	if (!info->rhdr.e_shdr) {
		return;
	}
	arch_update_purgatory(info);

	if (info->skip_checks) {
		unsigned int tmp = 1;

		elf_rel_set_symbol(&info->rhdr, "skip_checks", &tmp,
			sizeof(tmp));
		return;
	}

	memset(region, 0, sizeof(region));
	sha256_starts(&ctx);
	/* Compute a hash of the loaded kernel */
	for(j = i = 0; i < info->nr_segments; i++) {
		unsigned long nullsz;
		/* Don't include purgatory in the checksum.  The stack
		 * in the bss will definitely change, and the .data section
		 * will also change when we poke the sha256_digest in there.
		 * A very clever/careful person could probably improve this.
		 */
		if (info->segment[i].mem == (void *)info->rhdr.rel_addr) {
			continue;
		}

		/*
		 * Let architecture decide which segments to exclude from checksum
		 * if hotplug support is enabled.
		 */
		if (do_hotplug && arch_do_exclude_segment(info, &info->segment[i])) {
			dbgprintf("Skipping segment mem: 0x%lx from SHA calculation\n",
				  (unsigned long)info->segment[i].mem);
			continue;
		}

		sha256_update(&ctx, info->segment[i].buf,
			      info->segment[i].bufsz);
		nullsz = info->segment[i].memsz - info->segment[i].bufsz;
		while(nullsz) {
			unsigned long bytes = nullsz;
			if (bytes > sizeof(null_buf)) {
				bytes = sizeof(null_buf);
			}
			sha256_update(&ctx, null_buf, bytes);
			nullsz -= bytes;
		}
		region[j].start = (unsigned long) info->segment[i].mem;
		region[j].len   = info->segment[i].memsz;
		j++;
	}
	sha256_finish(&ctx, digest);
	elf_rel_set_symbol(&info->rhdr, "sha256_regions", &region,
			   sizeof(region));
	elf_rel_set_symbol(&info->rhdr, "sha256_digest", &digest,
			   sizeof(digest));
}

/*
 *	Load the new kernel
 */
static int my_load(const char *type, int fileind, int argc, char **argv,
		   unsigned long kexec_flags, int skip_checks, void *entry)
{
	char *kernel;
	char *kernel_buf;
	off_t kernel_size;
	int i = 0;
	int result;
	struct kexec_info info;
	long native_arch;
	int guess_only = 0;

	memset(&info, 0, sizeof(info));
	info.kexec_flags = kexec_flags;
	info.skip_checks = skip_checks;

	result = 0;
	if (argc - fileind <= 0) {
		fprintf(stderr, "No kernel specified\n");
		usage();
		return -1;
	}
	kernel = argv[fileind];
	/* slurp in the input kernel */
	kernel_buf = slurp_decompress_file(kernel, &kernel_size);

	dbgprintf("kernel: %p kernel_size: %#llx\n",
		  kernel_buf, (unsigned long long)kernel_size);

	if (get_memory_ranges(&info.memory_range, &info.memory_ranges,
		info.kexec_flags) < 0 || info.memory_ranges == 0) {
		fprintf(stderr, "Could not get memory layout\n");
		return -1;
	}
	/* if a kernel type was specified, try to honor it */
	if (type) {
		for (i = 0; i < file_types; i++) {
			if (strcmp(type, file_type[i].name) == 0)
				break;
		}
		if (i == file_types) {
			fprintf(stderr, "Unsupported kernel type %s\n", type);
			return -1;
		} else {
			/* make sure our file is really of that type */
			if (file_type[i].probe(kernel_buf, kernel_size) < 0)
				guess_only = 1;
		}
	}
	if (!type || guess_only) {
		for (i = 0; i < file_types; i++) {
			if (file_type[i].probe(kernel_buf, kernel_size) == 0)
				break;
		}
		if (i == file_types) {
			fprintf(stderr, "Cannot determine the file type "
					"of %s\n", kernel);
			return -1;
		} else {
			if (guess_only) {
				fprintf(stderr, "Wrong file type %s, "
					"file matches type %s\n",
					type, file_type[i].name);
				return -1;
			}
		}
	}
	/* Figure out our native architecture before load */
	native_arch = physical_arch();
	if (native_arch < 0) {
		return -1;
	}
	info.kexec_flags |= native_arch;

	result = file_type[i].load(argc, argv, kernel_buf, kernel_size, &info);
	if (result < 0) {
		switch (result) {
		case ENOCRASHKERNEL:
			fprintf(stderr,
				"No crash kernel segment found in /proc/iomem\n"
				"Please check the crashkernel= boot parameter.\n");
			break;
		case EFAILED:
		default:
			fprintf(stderr, "Cannot load %s\n", kernel);
			break;
		}
		return result;
	}
	/* If we are not in native mode setup an appropriate trampoline */
	if (arch_compat_trampoline(&info) < 0) {
		return -1;
	}
	if (info.kexec_flags & KEXEC_PRESERVE_CONTEXT) {
		add_backup_segments(&info, mem_min, mem_max - mem_min + 1);
	}
	/* Verify all of the segments load to a valid location in memory */
	for (i = 0; i < info.nr_segments; i++) {
		if (!valid_memory_segment(&info, info.segment +i)) {
			fprintf(stderr, "Invalid memory segment %p - %p\n",
				info.segment[i].mem,
				((char *)info.segment[i].mem) + 
				info.segment[i].memsz);
			return -1;
		}
	}
	/* Sort the segments and verify we don't have overlaps */
	if (sort_segments(&info) < 0) {
		return -1;
	}
	/* if purgatory is loaded update it */
	update_purgatory(&info);
	if (entry)
		info.entry = entry;

	dbgprintf("kexec_load: entry = %p flags = 0x%lx\n",
		  info.entry, info.kexec_flags);
	if (kexec_debug)
		print_segments(stderr, &info);

	if (xen_present())
		result = xen_kexec_load(&info);
	else
		result = kexec_load(info.entry,
				    info.nr_segments, info.segment,
				    info.kexec_flags);
	if (result != 0) {
		/* The load failed, print some debugging information */
		fprintf(stderr, "kexec_load failed: %s\n", 
			strerror(errno));
		fprintf(stderr, "entry       = %p flags = 0x%lx\n", 
			info.entry, info.kexec_flags);
		print_segments(stderr, &info);
	}
	return result;
}

static int kexec_file_unload(unsigned long kexec_file_flags)
{
	int ret = 0;

	if (!is_kexec_file_load_implemented())
		return EFALLBACK;

	ret = kexec_file_load(-1, -1, 0, NULL, kexec_file_flags);
	if (ret != 0) {
		if (errno == ENOSYS) {
			ret = EFALLBACK;
		} else {
			/*
			 * The unload failed, print some debugging
			 * information */
			fprintf(stderr, "kexec_file_load(unload) failed: %s\n",
				strerror(errno));
			ret = EFAILED;
		}
	}
	return ret;
}

static int k_unload (unsigned long kexec_flags)
{
	int result;
	long native_arch;

	/* set the arch */
	native_arch = physical_arch();
	if (native_arch < 0) {
		return -1;
	}
	kexec_flags |= native_arch;

	if (xen_present())
		result = xen_kexec_unload(kexec_flags);
	else
		result = kexec_load(NULL, 0, NULL, kexec_flags);
	if (result != 0) {
		/* The unload failed, print some debugging information */
		fprintf(stderr, "kexec unload failed: %s\n",
			strerror(errno));
	}
	return result;
}

/*
 *	Start a reboot.
 */
static int my_shutdown(void)
{
	char *args[] = {
		"shutdown",
		"-r",
		"now",
		NULL
	};

	execv("/sbin/shutdown", args);
	execv("/etc/shutdown", args);
	execv("/bin/shutdown", args);

	perror("shutdown");
	return -1;
}

/*
 *	Exec the new kernel. If successful, this triggers an immediate reboot
 *	and does not return, but Xen Live Update is an exception (more on this
 *	below).
 */
static int my_exec(void)
{
	if (xen_present()) {
		int ret;

		/*
		 * There are two cases in which the Xen hypercall may return:
		 * 1) An error occurred, e.g. the kexec image was not loaded.
		 *    The exact error is indicated by errno.
		 * 2) Live Update was successfully scheduled. Note that unlike
		 *    a normal kexec, Live Update happens asynchronously, i.e.
		 *    the hypercall merely schedules the kexec operation and
		 *    returns immediately.
		 */
		ret = xen_kexec_exec(kexec_flags);
		if ((kexec_flags & KEXEC_LIVE_UPDATE) && !ret)
			return 0;
	} else
		reboot(LINUX_REBOOT_CMD_KEXEC);
	/* I have failed if I make it here */
	fprintf(stderr, "kexec failed: %s\n", 
		strerror(errno));
	return -1;
}

static int load_jump_back_helper_image(unsigned long kexec_flags, void *entry)
{
	int result;
	struct kexec_segment seg;

	memset(&seg, 0, sizeof(seg));
	result = kexec_load(entry, 1, &seg, kexec_flags);
	return result;
}

static int kexec_loaded(const char *file)
{
	long ret = -1;
	FILE *fp;
	char *p;
	char line[3];

	/* No way to tell if an image is loaded under Xen, assume it is. */
	if (xen_present())
		return 1;

	fp = fopen(file, "r");
	if (fp == NULL)
		return -1;

	p = fgets(line, sizeof(line), fp);
	fclose(fp);

	if (p == NULL)
		return -1;

	ret = strtol(line, &p, 10);

	/* Too long */
	if (ret > INT_MAX)
		return -1;

	/* No digits were found */
	if (p == line)
		return -1;

	return (int)ret;
}

/*
 *	Jump back to the original kernel
 */
static int my_load_jump_back_helper(unsigned long kexec_flags, void *entry)
{
	int result;

	if (kexec_loaded(KEXEC_LOADED_PATH)) {
		fprintf(stderr, "There is kexec kernel loaded, make sure "
			"you are in kexeced kernel.\n");
		return -1;
	}
	if (!entry) {
		fprintf(stderr, "Please specify jump back entry "
			"in command line\n");
		return -1;
	}
	result = load_jump_back_helper_image(kexec_flags, entry);
	if (result) {
		fprintf(stderr, "load jump back kernel failed: %s\n",
			strerror(errno));
		return result;
	}
	return result;
}

static void version(void)
{
	printf(PACKAGE_STRING "\n");
}

void usage(void)
{
	int i;

	version();
	printf("Usage: kexec [OPTION]... [kernel]\n"
	       "Directly reboot into a new kernel\n"
	       "\n"
	       " -h, --help           Print this help.\n"
	       " -v, --version        Print the version of kexec.\n"
	       " -f, --force          Force an immediate kexec,\n"
	       "                      don't call shutdown.\n"
	       " -i, --no-checks      Fast reboot, no memory integrity checks.\n"
	       " -x, --no-ifdown      Don't bring down network interfaces.\n"
	       " -y, --no-sync        Don't sync filesystems before kexec.\n"
	       " -l, --load           Load the new kernel into the\n"
	       "                      current kernel.\n"
	       " -p, --load-panic     Load the new kernel for use on panic.\n"
	       " -u, --unload         Unload the current kexec target kernel.\n"
	       "                      If capture kernel is being unloaded\n"
	       "                      specify -p with -u.\n"
	       " -e, --exec           Execute a currently loaded kernel.\n"
               "     --exec-live-update Execute a currently loaded xen image after\n"
                                      "storing the state required to live update.\n"
	       " -t, --type=TYPE      Specify the new kernel is of this type.\n"
	       "     --mem-min=<addr> Specify the lowest memory address to\n"
	       "                      load code into.\n"
	       "     --mem-max=<addr> Specify the highest memory address to\n"
	       "                      load code into.\n"
	       "     --reuseinitrd    Reuse initrd from first boot.\n"
	       "     --print-ckr-size Print crash kernel region size.\n"
	       "     --load-preserve-context Load the new kernel and preserve\n"
	       "                      context of current kernel during kexec.\n"
	       "     --load-jump-back-helper Load a helper image to jump back\n"
	       "                      to original kernel.\n"
	       "     --load-live-update Load the new kernel to overwrite the\n"
	       "                      running kernel.\n"
	       "     --entry=<addr>   Specify jump back address.\n"
	       "                      (0 means it's not jump back or\n"
	       "                      preserve context)\n"
	       "                      to original kernel.\n"
	       " -s, --kexec-file-syscall Use file based syscall for kexec operation\n"
	       " -c, --kexec-syscall  Use the kexec_load syscall for for compatibility\n"
	       "                      with systems that don't support -s\n"
	       " -a, --kexec-syscall-auto  Use file based syscall for kexec and fall\n"
	       "                      back to the compatibility syscall when file based\n"
	       "                      syscall is not supported or the kernel did not\n"
	       "                      understand the image (default)\n"
	       " --hotplug            Do in-kernel update of kexec segments on CPU/Memory\n"
	       "                      hot add/remove events, avoiding the need to reload\n"
	       "                      kdump kernel on online/offline events.\n"
	       " -d, --debug          Enable debugging to help spot a failure.\n"
	       " -S, --status         Return 1 if the type (by default crash) is loaded,\n"
	       "                      0 if not.\n"
	       "\n"
	       "Supported kernel file types and options: \n");
	for (i = 0; i < file_types; i++) {
		printf("%s\n", file_type[i].name);
		file_type[i].usage();
	}
	printf(	"Architecture options: \n");
	arch_usage();
	printf("\n");
}

static int k_status(unsigned long kexec_flags)
{
	int result;
	long native_arch;

	/* set the arch */
	native_arch = physical_arch();
	if (native_arch < 0) {
		return -1;
	}
	kexec_flags |= native_arch;

	if (xen_present())
		result = xen_kexec_status(kexec_flags);
	else {
		if (kexec_flags & KEXEC_ON_CRASH)
			result = kexec_loaded(KEXEC_CRASH_LOADED_PATH);
		else
			result = kexec_loaded(KEXEC_LOADED_PATH);
	}
	return result;
}


/*
 * Remove parameter from a kernel command line. Helper function by get_command_line().
 */
void remove_parameter(char *line, const char *param_name)
{
	char *start, *end;

	start = strstr(line, param_name);

	/* parameter not found */
	if (!start)
		return;

	/*
	 * check if that's really the start of a parameter and not in
	 * the middle of the word
	 */
	if (start != line && !isspace(*(start-1)))
		return;

	end = strstr(start, " ");
	if (!end)
		*start = 0;
	else {
		memmove(start, end+1, strlen(end));
		*(end + strlen(end)) = 0;
	}
}

static ssize_t _read(int fd, void *buf, size_t count)
{
	ssize_t ret, offset = 0;

	do {
		ret = read(fd, buf + offset, count - offset);
		if (ret < 0) {
			if ((errno == EINTR) ||	(errno == EAGAIN))
				continue;
			return ret;
		}
		offset += ret;
	} while (ret && offset < count);

	return offset;
}

static char *slurp_proc_file(const char *filename, size_t *len)
{
	ssize_t ret, startpos = 0;
	unsigned int size = 64;
	char *buf = NULL, *tmp;
	int fd;

	fd = open(filename, O_RDONLY);
	if (fd == -1)
		return NULL;

	do {
		size *= 2;
		tmp = realloc(buf, size);
		if (!tmp) {
			free(buf);
			return NULL;
		}
		buf = tmp;

		ret = _read(fd, buf + startpos, size - startpos);
		if (ret < 0) {
			free(buf);
			return NULL;
		}

		startpos += ret;

	} while(ret);

	*len = startpos;
	return buf;
}

/*
 * Returns the contents of the current command line to be used with
 * --reuse-cmdline option.  The function gets called from architecture specific
 * code. If we load a panic kernel, that function will strip the
 * "crashkernel=" option because it does not make sense that the crashkernel
 * reserves memory for a crashkernel (well, it would not boot since the
 * amount is exactly the same as the crashkernel has overall memory). Also,
 * remove the BOOT_IMAGE from lilo (and others) since that doesn't make
 * sense here any more. The kernel could be different even if we reuse the
 * commandline.
 *
 * The function returns dynamically allocated memory.
 */
char *get_command_line(void)
{
	char *p, *line;
	size_t size;

	line = slurp_proc_file("/proc/cmdline", &size);
	if (!line || !size)
		die("Failed to read /proc/cmdline\n");

	/* strip newline */
	line[size-1] = '\0';

	p = strpbrk(line, "\r\n");
	if (p)
		*p = '\0';

	remove_parameter(line, "BOOT_IMAGE");
	if (kexec_flags & KEXEC_ON_CRASH)
		remove_parameter(line, "crashkernel");

	return line;
}

/* check we retained the initrd */
static void check_reuse_initrd(void)
{
	char *str = NULL;
	char *line = get_command_line();

	str = strstr(line, "retain_initrd");
	free(line);

	if (str == NULL)
		die("unrecoverable error: current boot didn't "
		    "retain the initrd for reuse.\n");
}

char *concat_cmdline(const char *base, const char *append)
{
	char *cmdline;
	if (!base && !append)
		return NULL;
	if (append && !base)
		return xstrdup(append);
	if (base && !append)
		return xstrdup(base);
	cmdline = xmalloc(strlen(base) + 1 + strlen(append) + 1);
	strcpy(cmdline, base);
	strcat(cmdline, " ");
	strcat(cmdline, append);
	return cmdline;
}

void cmdline_add_liveupdate(char **base)
{
	uint64_t lu_start, lu_end, lu_sizeM;
	char *str;
	char buf[64];
	size_t len;

	if ( !xen_present() )
		return;

	xen_get_kexec_range(KEXEC_RANGE_MA_LIVEUPDATE, &lu_start, &lu_end);
	lu_sizeM = (lu_end - lu_start) / (1024 * 1024) + 1;
	sprintf(buf, " liveupdate=%lluM@0x%llx", (unsigned long long)lu_sizeM,
		(unsigned long long)lu_start);
	len = strlen(*base) + strlen(buf) + 1;
	str = xmalloc(len);
	sprintf(str, "%s%s", *base, buf);
	*base = str;
}

/* New file based kexec system call related code */
static int do_kexec_file_load(int fileind, int argc, char **argv,
			unsigned long flags) {

	char *kernel;
	int kernel_fd, i;
	struct kexec_info info;
	int ret = 0;
	char *kernel_buf;
	off_t kernel_size;

	memset(&info, 0, sizeof(info));
	info.segment = NULL;
	info.nr_segments = 0;
	info.entry = NULL;
	info.backup_start = 0;
	info.kexec_flags = flags;

	info.file_mode = 1;
	info.kernel_fd = -1;
	info.initrd_fd = -1;

	if (!is_kexec_file_load_implemented())
		return EFALLBACK;

	if (argc - fileind <= 0) {
		fprintf(stderr, "No kernel specified\n");
		usage();
		return EFAILED;
	}

	kernel = argv[fileind];

	/* slurp in the input kernel */
	kernel_buf = slurp_decompress_file(kernel, &kernel_size);
	if (!kernel_buf) {
		fprintf(stderr, "Failed to decompress file %s:%s\n", kernel,
				strerror(errno));
		return EFAILED;
	}
	kernel_fd = copybuf_memfd(kernel_buf, kernel_size);
	if (kernel_fd < 0) {
		fprintf(stderr, "Failed to copy decompressed buf\n");
		return EFAILED;
	}

	for (i = 0; i < file_types; i++) {
		if (file_type[i].probe(kernel_buf, kernel_size) >= 0)
			break;
	}

	if (i == file_types) {
		fprintf(stderr, "Cannot determine the file type " "of %s\n",
				kernel);
		close(kernel_fd);
		return EFAILED;
	}

	ret = file_type[i].load(argc, argv, kernel_buf, kernel_size, &info);
	if (ret < 0) {
		fprintf(stderr, "Cannot load %s\n", kernel);
		close(kernel_fd);
		return ret;
	}

       /*
	* image type specific load functioin detect the capsule kernel type
	* and create another fd for file load. For example the zboot kernel.
	*/
	if (info.kernel_fd != -1)
		kernel_fd = info.kernel_fd;

	/*
	 * If there is no initramfs, set KEXEC_FILE_NO_INITRAMFS flag so that
	 * kernel does not return error with negative initrd_fd.
	 */
	if (info.initrd_fd == -1)
		info.kexec_flags |= KEXEC_FILE_NO_INITRAMFS;

	ret = kexec_file_load(kernel_fd, info.initrd_fd, info.command_line_len,
			info.command_line, info.kexec_flags);
	if (ret != 0) {
		switch (errno) {
			/*
			 * Something failed with signature verification.
			 * Reject the image.
			 */
		case ELIBBAD:
		case EKEYREJECTED:
		case ENOPKG:
		case ENOKEY:
		case EBADMSG:
		case EMSGSIZE:
			/* Reject by default. */
		default:
			fprintf(stderr, "kexec_file_load failed: %s\n", strerror(errno));
			ret = EFAILED;
			break;

			/* Not implemented. */
		case ENOSYS:
			/*
			 * Parsing image or other options failed
			 * The image may be invalid or image
			 * type may not supported by kernel so
			 * retry parsing in kexec-tools.
			 */
		case EINVAL:
		case ENOEXEC:
			/*
			 * ENOTSUP can be unsupported image
			 * type or unsupported PE signature
			 * wrapper type, duh.
			 */
		case ENOTSUP:
			ret = EFALLBACK;
			break;
		}
	}

	close(kernel_fd);
	return ret;
}

/*
 * Hotplug support for x86 in the kernel was added with the
 * `KEXEC_UPDATE_ELFCOREHDR` kexec bit, and later it was generalized
 * with the `KEXEC_CRASH_HOTPLUG_SUPPORT` kexec bit.
 *
 * Passing the `KEXEC_CRASH_HOTPLUG_SUPPORT` kexec bit to kernel
 * versions 6.5 to 6.9 on x86 with the `kexec_load` system call will
 * fail with `-EINVAL`.
 *
 * So for now, pass `KEXEC_UPDATE_ELFCOREHDR` for x86, and for other
 * architectures, use the `KEXEC_CRASH_HOTPLUG_SUPPORT` kexec bit. But
 * in the future, we can decide to get rid of `KEXEC_UPDATE_ELFCOREHDR`.
 *
 * NOTE: Xen KEXEC_LIVE_UPDATE and KEXEC_UPDATE_ELFCOREHDR collide
 */
static inline unsigned long get_hotplug_kexec_flag(void)
{
#if defined(__i386__) || defined(__x86_64__)
		return KEXEC_UPDATE_ELFCOREHDR;
#else
		return KEXEC_CRASH_HOTPLUG_SUPPORT;
#endif
}

static void print_crashkernel_region_size(void)
{
	uint64_t start = 0, end = 0;

	if (is_crashkernel_mem_reserved() &&
	    get_crash_kernel_load_range(&start, &end)) {
		fprintf(stderr, "get_crash_kernel_load_range() failed.\n");
		return;
	}

	printf("%" PRIu64 "\n", (start != end) ? (end - start + 1) : 0UL);
}

int main(int argc, char *argv[])
{
	int has_opt_load = 0;
	int do_load = 1;
	int do_exec = 0;
	int do_load_jump_back_helper = 0;
	int do_shutdown = 1;
	int do_sync = 1, skip_sync = 0;
	int do_ifdown = 0, skip_ifdown = 0;
	int do_unload = 0;
	int do_reuse_initrd = 0;
	int do_kexec_file_syscall = 1;
	int do_kexec_fallback = 1;
	int skip_checks = 0;
	int do_status = 0;
	void *entry = 0;
	char *type = 0;
	char *endptr;
	int opt;
	int result = 0;
	int fileind;
	static const struct option options[] = {
		KEXEC_ALL_OPTIONS
		{ 0, 0, 0, 0},
	};
	static const char short_options[] = KEXEC_ALL_OPT_STR;

	/* Reset getopt for the next pass. */
	opterr = 1;
	optind = 1;

	while ((opt = getopt_long(argc, argv, short_options,
				  options, 0)) != -1) {
		switch(opt) {
		case '?':
			usage();
			return 1;
		case OPT_HELP:
			usage();
			return 0;
		case OPT_VERSION:
			version();
			return 0;
		case OPT_DEBUG:
			kexec_debug = 1;
			kexec_file_flags |= KEXEC_FILE_DEBUG;
			break;
		case OPT_NOIFDOWN:
			skip_ifdown = 1;
			break;
		case OPT_NOSYNC:
			skip_sync = 1;
			break;
		case OPT_FORCE:
			do_load = 1;
			do_shutdown = 0;
			do_sync = 1;
			do_ifdown = 1;
			do_exec = 1;
			break;
		case OPT_LOAD:
			has_opt_load = 1;
			do_load = 1;
			do_exec = 0;
			do_shutdown = 0;
			break;
		case OPT_UNLOAD:
			do_load = 0;
			do_shutdown = 0;
			do_sync = 0;
			do_unload = 1;
			kexec_file_flags |= KEXEC_FILE_UNLOAD;
			break;
                case OPT_EXEC_LIVE_UPDATE:
			if ( !xen_present() ) {
				fprintf(stderr, "--exec-live-update only works under xen.\n");
                                return 1;
                        }
			kexec_flags |= KEXEC_LIVE_UPDATE;
			/* fallthrough */
		case OPT_EXEC:
			do_load = 0;
			do_shutdown = 0;
			do_sync = 1;
			do_ifdown = 1;
			do_exec = 1;
			break;
		case OPT_LOAD_JUMP_BACK_HELPER:
			do_load = 0;
			do_shutdown = 0;
			do_sync = 1;
			do_ifdown = 1;
			do_exec = 0;
			do_load_jump_back_helper = 1;
			kexec_flags = KEXEC_PRESERVE_CONTEXT;
			break;
		case OPT_ENTRY:
			entry = (void *)strtoul(optarg, &endptr, 0);
			if (*endptr) {
				fprintf(stderr,
					"Bad option value in --entry=%s\n",
					optarg);
				usage();
				return 1;
			}
			break;
		case OPT_LOAD_PRESERVE_CONTEXT:
		case OPT_LOAD_LIVE_UPDATE:
			do_load = 1;
			do_exec = 0;
			do_shutdown = 0;
			do_sync = 1;
			kexec_flags = (opt == OPT_LOAD_PRESERVE_CONTEXT) ?
				       KEXEC_PRESERVE_CONTEXT : KEXEC_LIVE_UPDATE;
			break;
		case OPT_TYPE:
			type = optarg;
			break;
		case OPT_PANIC:
			do_load = 1;
			do_exec = 0;
			do_shutdown = 0;
			do_sync = 0;
			kexec_file_flags |= KEXEC_FILE_ON_CRASH;
			kexec_flags = KEXEC_ON_CRASH;
			break;
		case OPT_MEM_MIN:
			mem_min = strtoul(optarg, &endptr, 0);
			if (*endptr) {
				fprintf(stderr,
					"Bad option value in --mem-min=%s\n",
					optarg);
				usage();
				return 1;
			}
			break;
		case OPT_MEM_MAX:
			mem_max = strtoul(optarg, &endptr, 0);
			if (*endptr) {
				fprintf(stderr,
					"Bad option value in --mem-max=%s\n",
					optarg);
				usage();
				return 1;
			}
			break;
		case OPT_REUSE_INITRD:
			do_reuse_initrd = 1;
			break;
		case OPT_KEXEC_FILE_SYSCALL:
			do_kexec_file_syscall = 1;
			do_kexec_fallback = 0;
			break;
		case OPT_KEXEC_SYSCALL:
			do_kexec_file_syscall = 0;
			do_kexec_fallback = 0;
			break;
		case OPT_KEXEC_SYSCALL_AUTO:
			do_kexec_file_syscall = 1;
			do_kexec_fallback = 1;
			break;
		case OPT_NOCHECKS:
			skip_checks = 1;
			break;
		case OPT_STATUS:
			do_status = 1;
			break;
		case OPT_PRINT_CKR_SIZE:
			print_crashkernel_region_size();
			return 0;
		case OPT_HOTPLUG:
			do_hotplug = 1;
			break;
		default:
			break;
		}
	}

	if (skip_ifdown)
		do_ifdown = 0;
	if (skip_sync)
		do_sync = 0;

	if (do_status) {
		if (kexec_flags == 0 && !has_opt_load)
			kexec_flags = KEXEC_ON_CRASH;
		do_load = 0;
		do_reuse_initrd = 0;
		do_unload = 0;
		do_load = 0;
		do_shutdown = 0;
		do_sync = 0;
		do_ifdown = 0;
		do_exec = 0;
		do_load_jump_back_helper = 0;
	}

	if (do_load &&
	    ((kexec_flags & KEXEC_ON_CRASH) ||
	     (kexec_file_flags & KEXEC_FILE_ON_CRASH)) &&
	    !is_crashkernel_mem_reserved()) {
		die("Memory for crashkernel is not reserved\n"
		    "Please reserve memory by passing"
		    "\"crashkernel=Y@X\" parameter to kernel\n"
		    "Then try to loading kdump kernel\n");
	}

	if (do_load && (kexec_flags & KEXEC_PRESERVE_CONTEXT) &&
	    mem_max == ULONG_MAX) {
		die("Please specify memory range used by kexeced kernel\n"
		    "to preserve the context of original kernel with \n"
		    "\"--mem-max\" parameter\n");
	}

	if (do_load && (kexec_flags & KEXEC_LIVE_UPDATE) &&
	    !xen_present()) {
		die("--load-live-update can only be used with xen\n");
	}

	if (do_hotplug) {
		const char *ces = "/sys/kernel/crash_elfcorehdr_size";
		char *buf, *endptr = NULL;
		off_t nread = 0;
		buf = slurp_file_len(ces, sizeof(buf)-1, &nread);
		if (buf) {
			if (buf[nread-1] == '\n')
				buf[nread-1] = '\0';
			elfcorehdrsz = strtoul(buf, &endptr, 0);
		}
		if (!elfcorehdrsz || (endptr && *endptr != '\0'))
			die("Path %s does not exist, the kernel needs CONFIG_CRASH_HOTPLUG\n", ces);
		dbgprintf("ELFCOREHDR_SIZE %lu\n", elfcorehdrsz);
		/* Indicate to the kernel it is ok to modify the relevant kexec segments */

		kexec_flags |= get_hotplug_kexec_flag();

	}

	fileind = optind;
	/* Reset getopt for the next pass; called in other source modules */
	opterr = 1;
	optind = 1;

	result = arch_process_options(argc, argv);

	/* Check for bogus options */
	if (!do_load) {
		while((opt = getopt_long(argc, argv, short_options,
					 options, 0)) != -1) {
			if ((opt == '?') || (opt >= OPT_ARCH_MAX)) {
				usage();
				return 1;
			}
		}
	}
	if (xen_present()) {
		do_kexec_file_syscall = 0;
		do_kexec_fallback = 0;
	}
	if (do_kexec_file_syscall) {
		if (do_load_jump_back_helper && !do_kexec_fallback)
			die("--load-jump-back-helper not supported with kexec_file_load\n");
		if (kexec_flags & KEXEC_PRESERVE_CONTEXT)
			die("--load-preserve-context not supported with kexec_file_load\n");
	}

	if (do_reuse_initrd){
		check_reuse_initrd();
		arch_reuse_initrd();
	}
	if (do_status) {
		result = k_status(kexec_flags);
	}
	if (do_unload) {
		if (do_kexec_file_syscall) {
			result = kexec_file_unload(kexec_file_flags);
			if (result == EFALLBACK && do_kexec_fallback) {
				/* Reset getopt for fallback */
				opterr = 1;
				optind = 1;
				do_kexec_file_syscall = 0;
			}
		}
		if (!do_kexec_file_syscall)
			result = k_unload(kexec_flags);
	}
	if (do_load && (result == 0)) {
		if (do_kexec_file_syscall) {
			result = do_kexec_file_load(fileind, argc, argv,
						 kexec_file_flags);
			if (result == EFALLBACK && do_kexec_fallback) {
				/* Reset getopt for fallback */
				opterr = 1;
				optind = 1;
				do_kexec_file_syscall = 0;
			}
		}
		if (!do_kexec_file_syscall)
			result = my_load(type, fileind, argc, argv,
						kexec_flags, skip_checks, entry);
	}
	/* Don't shutdown unless there is something to reboot to! */
	if ((result == 0) && (do_shutdown || do_exec) && !kexec_loaded(KEXEC_LOADED_PATH)) {
		die("Nothing has been loaded!\n");
	}
	if ((result == 0) && do_shutdown) {
		result = my_shutdown();
	}
	if ((result == 0) && do_sync) {
		sync();
	}
	if ((result == 0) && do_ifdown) {
		ifdown();
	}
	if ((result == 0) && do_exec) {
		result = my_exec();
	}
	if ((result == 0) && do_load_jump_back_helper) {
		result = my_load_jump_back_helper(kexec_flags, entry);
	}
	if (result == EFALLBACK)
		fputs("syscall kexec_file_load not available.\n", stderr);

	fflush(stdout);
	fflush(stderr);
	return result;
} 
