#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "memlib.h"

// NOTE: copied from kheap
#define PAGE_SIZE 4096
#if defined(__GNUC__)
#if __LP64__
#define PAGE_FRAME 0xfffffffffffff000
#else
#define PAGE_FRAME 0xfffff000
#endif /* __X86_64__ */
#endif /* __GNUC__ */

#define NSIZES 9
static const size_t sizes[NSIZES] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048};

#define SMALLEST_SUBPAGE_SIZE 8
#define LARGEST_SUBPAGE_SIZE 2048

typedef ptrdiff_t vaddr_t;
////////////////////////////////////////

#define CACHESIZE 128 /* Should cover most machines. */ // NOTE: copied from Ex2
#define MB 1048576										// 2^20 bytes
#define KB 1024											// 2^10 bytes
#define SEGMENT_SIZE 4 * MB

struct block_t
{
	struct block_t *next;
};

struct big_freelist
{
	int npages;
	struct big_freelist *next;
};

struct page
{
	struct block_t *free;		 // pointer to first free block
	struct block_t *local_free;	 // pointer to first deferred free by owner thread
	struct block_t *thread_free; // pointer to first deferred free by non-owner thread
	struct page_area *page_area; // pointer to start of page area

	size_t num_used;		 // number of blocks used (used for freeing pages)
	size_t num_thread_freed; // Number of blocks freed by other threads (used for freeing pages)

	void *capacity; // end of last usable block
	void *reserved; // end of page area (such that entire segment is 4MB-aligned)
	// DEBUG INFO
	size_t size_class; // return of size_class
	size_t block_size; // size_class + sizeof(block_t)
} typedef page;

// Array of blocks
struct page_area
{

} typedef page_area;

enum page_kind_enum
{
	SMALL, // 64KB 8-1024 bytes
	LARGE, // objects under 512KB, 1 large page that spans whole segment
	HUGE,  // objects over 512KB
};
struct segment
{
	size_t cpu_id;				   // owner CPUID
	uint32_t page_shift;		   // for small pages this is 16 (= 64KiB), while for large and huge pages it is 22 (= 4MiB) such that the index is always zero in those cases (as there is just one page)
	enum page_kind_enum page_kind; // page kind based on blocksize
	size_t total_num_pages;		   //  1 page if kind= large or huge, 64 equal-sized pages if small
	// TODO figure out if we want to store segment metadata independent of page
	// since small pages are 64KiB and 64 per segment.
	size_t num_used_pages; // sum of used + free should = total_num_pages
	size_t num_free_pages;
	page *free_pages; // only relevant for small pages
	page *pages;	  // pointer to first page
} __attribute__((aligned(SEGMENT_SIZE))) /*NOTE: this only works on gcc*/ typedef segment;

#define NUM_DIRECT_PAGES 127
#define NUM_PAGES 16
struct thread_heap
{
	uint8_t init; // 8
	// 8, 16, 24... -> 1024, step 8
	page *pages_direct[NUM_DIRECT_PAGES]; // 8

	// 2^3 -> 2^ 19
	page *pages[NUM_PAGES];		//
	size_t cpu_id;				// CPU number
	page *small_page_refs;		// freelist of unallocated small page refs
	segment *free_segment_refs; // linked list of freed segments that can be written to
	// check before allocating new segment with mem_sbrk
	// TODO fix padding
	uint8_t padding[CACHESIZE - 40]; // ensure that false sharing does not occur for this
} typedef thread_heap;

void init_thread_heap(size_t id)
{
}

thread_heap *get_heap(size_t id)
{
}

void deferred_free() {}

// TODO make this deterministic, getting size class should just be 2 calculations
size_t static inline size_class(size_t sz)
{
	unsigned i;
	for (i = 0; i < NSIZES; i++)
	{
		if (sz <= sizes[i])
		{
			return i;
		}
	}

	printf("Subpage allocator cannot handle allocation of size %lu\n",
		   (unsigned long)sz);
	exit(1);

	// keep compiler happy
	return 0;
}

void page_collect(page *page)
{
	page->free = page->local_free; // move the local num_free_pages list
	page->local_free = NULL;

	// move the thread num_free_pages list atomically
	// TODO
}

enum page_kind_enum get_page_type(size_t size)
{
	if (size <= 1024)
	{
		return SMALL;
	}
	if (size < 512 * KB)
	{
		return LARGE;
	}
	return HUGE;
}

#define SEGMENT_METADATA_SIZE 128
#define PAGE_METADATA_SIZE 128
segment *malloc_segment(thread_heap *heap, size_t size)
{
	// call mem_sbrk here
	segment *new_seg = mem_sbrk(SEGMENT_SIZE);

	// c cpu_id;
	// uint32_t page_shift; // for small pages this is 16 (= 64KiB), while for large and huge pages it is 22 (= 4MiB) such that the index is always zero in those cases (as there is just one page)
	// enum page_kind_enum page_kind;
	// size_t num_used_pages;
	// size_t num_free_pages;
	// page *pages;

	new_seg->cpu_id = heap->cpu_id;
	enum page_kind_enum page_kind = get_page_type(size);
	new_seg->page_kind = page_kind;

	// (From paper)...
	// we can calculate the page index by taking the difference and shifting by the
	// segment page_shift: for small pages this is 16 (= 64KiB), while for large and
	// huge pages it is 22 (= 4MiB) such that the index is always zero in those cases
	// (as there is just one page)

	new_seg->page_shift = page_kind == SMALL ? 16 : 22;
	new_seg->num_used_pages = 0;
	new_seg->num_free_pages = page_kind == SMALL ? 64 : 1; // 64 pages in small, 1 in others
	new_seg->total_num_pages = page_kind == SMALL ? 64 : 1;

	// pointer to start of pages.
	new_seg->pages = new_seg + sizeof(segment); // TODO add padding
	new_seg->free_pages = new_seg->pages;		// TODO: initialize all pages to be a ptr to next page

	// TODO: DETERMINE PAGE AREA POINTER BASED ON SIZEOF METADATA
	// for all pages, multiply page metadata by # num pages and add size of segment metadata
	// set the page area ptr to this value
	// make sure it doesnt have some weird alignment
	// make sure the area_ptr + #num_pages * area_size <= segment_ptr + 4mb, make sure stuff is within segment

	return new_seg;
}

// create page
page *malloc_page(thread_heap *heap, size_t size)
{

	// check to see if there is a num_free_pages page in segment we want to go to.
	// there is space for num_free_pages page....

	// if there is no num_free_pages page...
	// case where new segment is needed
	segment *segment = malloc_segment(heap, size);

	// assume we have valid page pointer to create
	page *page = segment->free_pages;
	// segment->free_pages = segment->free_pages->next;//TODO: change the page_kind to be a linked list, so that it has a next ptr

	// // DEBUG INFO
	page->block_size = size_class(size) + sizeof(struct block_t);

	page->num_used = 0;
	page->page_area = page + sizeof(page);

	page->local_free = NULL;
	page->thread_free = NULL;
	page->num_thread_freed = 0; // Number of blocks freed by other threads

	// logic for getting number of blocks
	// each block_t takes up sizeof(block_t) + size_class(size) = blocksize
	// capacity needs to align with segment size (4 MB)
	// sizeof(segment) + (sizeof(page) + capacity * block_size) * num_pages <= 4MB
	// alternatively if segment metadata is NOT part of the page
	// (sizeof(page) + capacity * block_size) * num_pages <= 4MB
	// capacity <= (((4MB - sizeof(segment))/num_pages) - sizeof(page))/block_size
	// segment->page_kind
	
	size_t space_per_page = ((4 * MB - sizeof(struct segment)) / segment->total_num_pages);
	size_t num_blocks = (space_per_page - sizeof(struct page)) / page->block_size; // end of last usable block.
	// TODO check that this is doing pointer arithmetic properly
	size_t usable_page_area = num_blocks * page->block_size;
	page->capacity = page->page_area + usable_page_area;
	page->reserved;

	// set up block_t freelist
	//  page-> fre is start of freelist
	struct block_t *curr_block = page->page_area;
	for (int i = 0; i < num_blocks; i++)
	{
		// next block should be block_size + next pointer away from current block
		struct block_t *tmp = curr_block + page->block_size;
	}
	page->free = NULL;
}

void *malloc_generic(thread_heap *heap, size_t size)
{
	deferred_free();

	//
	// for (page *pg = heap->pages[]) {

	// }

	// page/segment alloc path.
}

void *malloc_small(thread_heap *heap, size_t size)
{
	return malloc_generic(heap, size);
}

size_t get_cpuid()
{
	return 1;
}

void *mm_malloc(size_t sz)
{
	(void)sz; /* Avoid warning about unused variable */
	// return malloc(sz);
	// get CPU ID via syscall
	size_t cpu_id = get_cpuid();

	// if local thread heap is not initialized
	if (!tlb[cpu_id].init)
	{
		tlb[cpu_id].init = 1;
		memset(tlb[cpu_id].pages_direct, 0, sizeof(page *) * NUM_DIRECT_PAGES);
		memset(tlb[cpu_id].pages, 0, sizeof(page *) * NUM_PAGES);
		tlb[cpu_id].init = cpu_id;
		tlb[cpu_id].free_segment_refs = NULL;
		tlb[cpu_id].small_page_refs = NULL;
	}

	if (sz <= 8 * KB)
	{
		return malloc_small(&tlb[cpu_id], sz);
	}
	return malloc_generic(&tlb[cpu_id], sz);

	// return NULL;
}

void mm_free(void *ptr)
{
	(void)ptr; /* Avoid warning about unused variable */
	num_free_pages(ptr);
}

#define NUM_CPUS 40
// pointers to thread-local heaps
thread_heap tlb[NUM_CPUS];

int mm_init(void)
{
	if (dseg_lo == NULL && dseg_hi == NULL)
	{
		for (int i = 0; i < NUM_CPUS; i++)
		{
			tlb[i].init = 0;
		}
		return mem_init();
	}
	return 0;
}
