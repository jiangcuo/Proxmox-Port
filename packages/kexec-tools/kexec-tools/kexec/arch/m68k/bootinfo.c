#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

#include "../../kexec.h"

#include "bootinfo.h"

const char *bootinfo_file = DEFAULT_BOOTINFO_FILE;
static struct bi_rec *bootinfo;
static off_t bootinfo_size;

static unsigned int num_memchunks;

static struct bi_rec *bi_next(struct bi_rec *bi, uint16_t size)
{
	return (void *)((unsigned long)bi + size);
}

static struct bi_rec *bi_find(struct bi_rec *prev, uint16_t tag)
{
	struct bi_rec *bi = prev ? bi_next(prev, prev->size) : bootinfo;

	for (bi = prev ? bi_next(prev, prev->size) : bootinfo;
	     bi->tag != BI_LAST; bi = bi_next(bi, bi->size))
		if (bi->tag == tag)
			return bi;
	return NULL;
}

static void bi_remove(uint16_t tag)
{
	struct bi_rec *bi;
	off_t rem;
	uint16_t size;

	bi = bootinfo;
	rem = bootinfo_size;
	while (1) {
		if (bi->tag == BI_LAST)
			break;

		size = bi->size;
		if (bi->tag == tag) {
			memmove(bi, bi_next(bi, size), rem - size);
			bootinfo_size -= size;
			rem -= size;
			continue;
		}

		bi = bi_next(bi, size);
		rem -= size;
	}
}

static struct bi_rec *bi_add(uint16_t tag, uint16_t size)
{
	struct bi_rec *bi;

	/* Add 4-byte header and round up to multiple of 4 bytes */
	size = _ALIGN_UP(4 + size, 4);

	bootinfo = xrealloc(bootinfo, bootinfo_size + size);

	/* Replace old sentinel by new record */
	bi = bi_next(bootinfo, bootinfo_size - 2);
	bootinfo_size += size;
	memset(bi, 0, size);
	bi->tag = tag;
	bi->size = size;

	/* Re-add sentinel */
	bi_next(bi, size)->tag = BI_LAST;

	return bi;
}

void bootinfo_load(void)
{
	struct bi_rec *bi;
	off_t rem;
	uint16_t tag, size;

	dbgprintf("Loading bootinfo from %s\n", bootinfo_file);
	bootinfo = (void *)slurp_file_len(bootinfo_file, MAX_BOOTINFO_SIZE,
					  &bootinfo_size);
	if (!bootinfo)
		die("No bootinfo\n");

	bi = bootinfo;
	rem = bootinfo_size;
	while (1) {
		if (rem < 2)
			die("Unexpected end of bootinfo\n");

		tag = bi->tag;
		if (tag == BI_LAST) {
			rem -= 2;
			break;
		}

		if (rem < 4)
			die("Unexpected end of bootinfo\n");

		size = bi->size;
		if (size < 4 || size % 4)
			die("Invalid tag size\n");
		if (rem < size)
			die("Unexpected end of bootinfo\n");

		if (tag == BI_MEMCHUNK)
			num_memchunks++;

		bi = bi_next(bi, size);
		rem -= size;
	}

	if (rem)
		die("Trailing data at end of bootinfo\n");
}

void bootinfo_print(void)
{
	struct bi_rec *bi = bootinfo;
	uint16_t tag, size;

	while (1) {
		tag = bi->tag;
		if (tag == BI_LAST) {
			puts("BI_LAST");
			break;
		}

		size = bi->size;
		switch (tag) {
		case BI_MACHTYPE:
			printf("BI_MACHTYPE: 0x%08x\n", bi->machtype);
			break;

		case BI_MEMCHUNK:
			printf("BI_MEMCHUNK: 0x%08x bytes at 0x%08x\n",
			       bi->mem_info.size, bi->mem_info.addr);
			break;

		case BI_RAMDISK:
			printf("BI_RAMDISK: 0x%08x bytes at 0x%08x\n",
			       bi->mem_info.size, bi->mem_info.addr);
			break;

		case BI_COMMAND_LINE:
			printf("BI_COMMAND_LINE: %s\n", bi->string);
			break;

		case BI_RNG_SEED:
			/* These are secret, so never print them to the console */
			printf("BI_RNG_SEED: 0x%08x bytes\n", be16_to_cpu(bi->rng_seed.len));
			break;

		default:
			printf("BI tag 0x%04x size %u\n", tag, size);
			break;
		}
		bi = bi_next(bi, size);
	}
}

int bootinfo_get_memory_ranges(struct memory_range **range)
{
	struct memory_range *ranges;
	unsigned int i;
	struct bi_rec *bi;

	ranges = xmalloc(num_memchunks * sizeof(struct memory_range));
	for (i = 0, bi = NULL;
	     i < num_memchunks && (bi = bi_find(bi, BI_MEMCHUNK)); i++) {
		ranges[i].start = bi->mem_info.addr;
		ranges[i].end = bi->mem_info.addr + bi->mem_info.size - 1;
		ranges[i].type = RANGE_RAM;
	}

	*range = ranges;
	return i;
}

void bootinfo_set_cmdline(const char *cmdline)
{
	struct bi_rec *bi;
	uint16_t size;

	/* Remove existing command line records */
	bi_remove(BI_COMMAND_LINE);

	if (!cmdline)
		return;

	/* Add new command line record */
	size = strlen(cmdline) + 1;
	bi = bi_add(BI_COMMAND_LINE, size);
	memcpy(bi->string, cmdline, size);
}

void bootinfo_set_ramdisk(unsigned long ramdisk_addr,
			  unsigned long ramdisk_size)
{
	struct bi_rec *bi;

	/* Remove existing ramdisk records */
	bi_remove(BI_RAMDISK);

	if (!ramdisk_size)
		return;

	/* Add new ramdisk record */
	bi = bi_add(BI_RAMDISK, sizeof(bi->mem_info));
	bi->mem_info.addr = ramdisk_addr;
	bi->mem_info.size = ramdisk_size;
}

void bootinfo_add_rng_seed(void)
{
	enum { RNG_SEED_LEN = 32 };
	struct bi_rec *bi;

	/* Remove existing rng seed records */
	bi_remove(BI_RNG_SEED);

	/* Add new rng seed record */
	bi = bi_add(BI_RNG_SEED, sizeof(bi->rng_seed) + RNG_SEED_LEN);
	if (getrandom(bi->rng_seed.data, RNG_SEED_LEN, GRND_NONBLOCK) != RNG_SEED_LEN) {
		bi_remove(BI_RNG_SEED);
		return;
	}
	bi->rng_seed.len = cpu_to_be16(RNG_SEED_LEN);
}


    /*
     * Check the bootinfo version in the kernel image
     * All failures are non-fatal, as kexec may be used to load
     * non-Linux images
     */

void bootinfo_check_bootversion(const struct kexec_info *info)
{
	struct bi_rec *bi;
	const struct bootversion *bv;
	uint16_t major, minor;
	unsigned int i;

	bv = info->segment[0].buf;
	if (bv->magic != BOOTINFOV_MAGIC) {
		printf("WARNING: No bootversion in kernel image\n");
		return;
	}

	bi = bi_find(NULL, BI_MACHTYPE);
	if (!bi) {
		printf("WARNING: No machtype in bootinfo\n");
		return;
	}

	for (i = 0; bv->machversions[i].machtype != bi->machtype; i++)
		if (!bv->machversions[i].machtype) {
			printf("WARNING: Machtype 0x%08x not in kernel bootversion\n",
			       bi->machtype);
			return;
		}

	major = BI_VERSION_MAJOR(bv->machversions[i].version);
	minor = BI_VERSION_MINOR(bv->machversions[i].version);
	dbgprintf("Kernel uses bootversion %u.%u\n", major, minor);
	if (major != SUPPORTED_BOOTINFO_VERSION)
		printf("WARNING: Kernel bootversion %u.%u is too %s for this kexec (expected %u.x)\n",
		       major, minor,
		       major < SUPPORTED_BOOTINFO_VERSION ? "old" : "new",
		       SUPPORTED_BOOTINFO_VERSION);
}

void add_bootinfo(struct kexec_info *info, unsigned long addr)
{
	add_buffer(info, bootinfo, bootinfo_size, bootinfo_size,
		   sizeof(void *), addr, 0x0fffffff, 1);
}
