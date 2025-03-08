#ifndef MEM_REGIONS_H
#define MEM_REGIONS_H

struct memory_ranges;
struct memory_range;

void mem_regions_sort(struct memory_ranges *ranges);

int mem_regions_exclude(struct memory_ranges *ranges,
			const struct memory_range *range);

int mem_regions_add(struct memory_ranges *ranges, unsigned long long base,
                    unsigned long long length, int type);

int mem_regions_alloc_and_exclude(struct memory_ranges *ranges,
				  const struct memory_range *range);

int mem_regions_alloc_and_add(struct memory_ranges *ranges,
			      unsigned long long base,
			      unsigned long long length, int type);

#endif
