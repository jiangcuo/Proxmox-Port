#include <stdlib.h>

#include "kexec.h"
#include "mem_regions.h"

static int mem_range_cmp(const void *a1, const void *a2)
{
	const struct memory_range *r1 = a1;
	const struct memory_range *r2 = a2;

	if (r1->start > r2->start)
		return 1;
	if (r1->start < r2->start)
		return -1;

	return 0;
}

/**
 * mem_regions_sort() - sort ranges into ascending address order
 * @ranges: ranges to sort
 *
 * Sort the memory regions into ascending address order.
 */
void mem_regions_sort(struct memory_ranges *ranges)
{
	qsort(ranges->ranges, ranges->size, sizeof(*ranges->ranges),
	      mem_range_cmp);
}

/**
 * mem_regions_add() - add a memory region to a set of ranges
 * @ranges: ranges to add the memory region to
 * @base: base address of memory region
 * @length: length of memory region in bytes
 * @type: type of memory region
 *
 * Add the memory region to the set of ranges, and return %0 if successful,
 * or %-1 if we ran out of space.
 */
int mem_regions_add(struct memory_ranges *ranges, unsigned long long base,
                    unsigned long long length, int type)
{
	struct memory_range *range;

	if (ranges->size >= ranges->max_size)
		return -1;

	range = ranges->ranges + ranges->size++;
	range->start = base;
	range->end = base + length - 1;
	range->type = type;

	return 0;
}

static void mem_regions_remove(struct memory_ranges *ranges, int index)
{
	int tail_entries;

	/* we are assured to have at least one entry */
	ranges->size -= 1;

	/* if we have following entries, shuffle them down one place */
	tail_entries = ranges->size - index;
	if (tail_entries)
		memmove(ranges->ranges + index, ranges->ranges + index + 1,
			tail_entries * sizeof(*ranges->ranges));

	/* zero the new tail entry */
	memset(ranges->ranges + ranges->size, 0, sizeof(*ranges->ranges));
}

/**
 * mem_regions_exclude() - excludes a memory region from a set of memory ranges
 * @ranges: memory ranges to exclude the region from
 * @range: memory range to exclude
 *
 * Exclude a memory region from a set of memory ranges.  We assume that
 * the region to be excluded is either wholely located within one of the
 * memory ranges, or not at all.
 */
int mem_regions_exclude(struct memory_ranges *ranges,
			const struct memory_range *range)
{
	int i, ret;

	for (i = 0; i < ranges->size; i++) {
		struct memory_range *r = ranges->ranges + i;

		/*
		 * We assume that crash area is fully contained in
		 * some larger memory area.
		 */
		if (r->start <= range->start && r->end >= range->end) {
			if (r->start == range->start) {
				if (r->end == range->end)
					/* Remove this entry */
					mem_regions_remove(ranges, i);
				else
					/* Shrink the start of this memory range */
					r->start = range->end + 1;
			} else if (r->end == range->end) {
				/* Shrink the end of this memory range */
				r->end = range->start - 1;
			} else {
				/*
				 * Split this area into 2 smaller ones and
				 * remove excluded range from between. First
				 * create new entry for the remaining area.
				 */
				ret = mem_regions_add(ranges, range->end + 1,
						      r->end - range->end, 0);
				if (ret < 0)
					return ret;

				/*
				 * Update this area to end before excluded
				 * range.
				 */
				r->end = range->start - 1;
				break;
			}
		}
	}
	return 0;
}

#define KEXEC_MEMORY_RANGES 16

int mem_regions_alloc_and_add(struct memory_ranges *ranges,
			      unsigned long long base,
			      unsigned long long length, int type)
{
	void *new_ranges;

	if (ranges->size >= ranges->max_size) {
		new_ranges = realloc(ranges->ranges,
				sizeof(struct memory_range) *
				(ranges->max_size + KEXEC_MEMORY_RANGES));
		if (!new_ranges)
			return -1;

		ranges->ranges = new_ranges;
		ranges->max_size += KEXEC_MEMORY_RANGES;
	}

	return mem_regions_add(ranges, base, length, type);
}

int mem_regions_alloc_and_exclude(struct memory_ranges *ranges,
				  const struct memory_range *range)
{
	void *new_ranges;

	/* for safety, we should have at least one free entry in ranges */
	if (ranges->size >= ranges->max_size) {
		new_ranges = realloc(ranges->ranges,
				sizeof(struct memory_range) *
				(ranges->max_size + KEXEC_MEMORY_RANGES));
		if (!new_ranges)
			return -1;

		ranges->ranges = new_ranges;
		ranges->max_size += KEXEC_MEMORY_RANGES;
	}

	return mem_regions_exclude(ranges, range);
}
