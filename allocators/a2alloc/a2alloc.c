#include <stdlib.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include "memlib.h"
#include <assert.h>

// NOTE: copied from kheap
#define PAGE_SIZE 4096
#if defined(__GNUC__)
#if __LP64__
#define PAGE_FRAME 0xfffffffffffff000
#else
#define PAGE_FRAME 0xfffff000
#endif /* __X86_64__ */
#endif /* __GNUC__ */


#define SMALLEST_SUBPAGE_SIZE 8
#define LARGEST_SUBPAGE_SIZE 4096

typedef ptrdiff_t vaddr_t;
////////////////////////////////////////

//DESIGN DEVIATIONS/INTERPRETATIONS/ASSUMPTIONS
//1: pages direct goes from 8 to 512
//2: multiple of 8 block sizes are a subset of the nearest power of 2 block size in pages (eg, 24 is in 32's list in pages)
//3: first page area is smaller than the rest to fit metadata from pages and the segment

#define NSIZES 9
static const size_t pages_sizes[NSIZES] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288};

#define NUM_TOTAL_SIZES 74
static const size_t all_sizes[NUM_TOTAL_SIZES] = {8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248, 256, 264, 272, 280, 288, 296, 304, 312, 320, 328, 336, 344, 352, 360, 368, 376, 384, 392, 400, 408, 416, 424, 432, 440, 448, 456, 464, 472, 480, 488, 496, 504, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288 };

#define CACHESIZE 128 /* Should cover most machines. */ // NOTE: copied from Ex2
#define MB 1048576										// 2^20 bytes
#define KB 1024											// 2^10 bytes


#define SEGMENT_SIZE 4 * MB
#define NUM_PAGES_SMALL_SEGMENT 64

struct block_t
{
	struct block_t *next;
};

struct big_freelist
{
	int npages;
	struct big_freelist *next;
};

struct page_reference
{
	struct page_reference *next;
	struct page *page;
};

struct page
{
	struct block_t *free;		 // pointer to first free block
	struct block_t *local_free;	 // pointer to first deferred free by owner thread
	struct block_t *thread_free; // pointer to first deferred free by non-owner thread
	struct page_area *page_area; // pointer to start of page area

	size_t num_used;		 // number of blocks used (used for freeing pages)
	size_t num_thread_freed; // Number of blocks freed by other threads (used for freeing pages)
	size_t total_num_blocks; // total number of blocks


	char *capacity; // end of last usable block
	char *reserved; // end of page area (such that entire segment is 4MB-aligned)
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
	page pages[NUM_PAGES_SMALL_SEGMENT];	  // pointer to array of page metadata (can be size 1)
// } __attribute__((aligned(SEGMENT_SIZE))) /*NOTE: this only works on gcc*/ typedef segment;
} /*NOTE: this only works on gcc*/ typedef segment;

#define NUM_DIRECT_PAGES 127
#define NUM_PAGES 16
struct thread_heap
{
	uint8_t init; // 8
	// 8, 16, 24... -> 1024, step 8
	struct page *pages_direct[NUM_DIRECT_PAGES]; // 8

	// 2^3 -> 2^ 19
	struct page_reference *pages[NUM_PAGES];		//
	size_t cpu_id;				// CPU number
	page *small_page_refs;		// freelist of unallocated small page refs
	struct segment *free_segment_refs; // linked list of freed segments that can be written to
	// check before allocating new segment with mem_sbrk
	// TODO fix padding
	uint8_t padding[CACHESIZE - 40]; // ensure that false sharing does not occur for this
} typedef thread_heap;

#define NUM_CPUS 40
// pointers to thread-local heaps
thread_heap tlb[NUM_CPUS];

void init_thread_heap(size_t id)
{
}

thread_heap *get_heap(size_t id)
{
}

int64_t static inline best_fit_index(size_t size, size_t sizes_array[], size_t len) {
	//ceil div to get number of blocks needed, then multiply by block size
	//TODO: change this to use a static array of predefined pages_sizes
	unsigned i;
	for (i = 0; i < len; i++)
	{
		if (size <= sizes_array[i])
		{
			return i;
		}
	}
	return -1;
}

//gets the appropriate index for the block size that fits <size> in our all_sizes array
//NOTE: all items up until and including 1024, have indexes that align with pages_direct's indexes
//		so, this can be used to get an index for that
// size_t static inline nearest_block_size_index(size_t size) {
// 	//ceil div to get number of blocks needed, then multiply by block size
// 	//TODO: change this to use a static array of predefined pages_sizes
// 	unsigned i;
// 	for (i = 0; i < NUM_TOTAL_SIZES; i++)
// 	{
// 		if (size <= all_sizes[i])
// 		{
// 			return i;
// 		}
// 	}
// }
#define nearest_block_size_index(size) best_fit_index(size, all_sizes, NUM_TOTAL_SIZES)
#define pages_direct_index(size) best_fit_index(size, all_sizes, NUM_DIRECT_PAGES)

//NOTE: in pages (the linked list) the list for 32 block size has to be able to hold 24 block size pages, otherwise we would not be able
//		to have 24 block sized pages in pages direct (more than 1 at least)
size_t static inline nearest_block_size(size_t size) {
	//ceil div to get number of blocks needed, then multiply by block size
	//TODO: change this to use a static array of predefined pages_sizes
	return all_sizes[nearest_block_size_index(size)];
}


// TODO make this deterministic, getting size class should just be 2 calculations
///gets the appropriate index for pages
// size_t static inline size_class(size_t sz)
// {
// 	unsigned i;
// 	for (i = 0; i < NSIZES; i++)
// 	{
// 		if (sz <= pages_sizes[i])
// 		{
// 			return i;
// 		}
// 	}
#define size_class(size) best_fit_index(size, pages_sizes, NSIZES)

// 	printf("Subpage allocator cannot handle allocation of size %lu\n",
// 		   (unsigned long)sz);
// 	exit(1);

// 	// keep compiler happy
// 	return 0;
// }

void page_collect(page *page)
{
	page->free = page->local_free; // move the local num_free_pages list
	page->local_free = NULL;

	// move the thread num_free_pages list atomically
	// TODO
}

enum page_kind_enum get_page_kind(size_t size)
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

struct block_t *create_free_blocks(struct page *page, size_t num_blocks)
{
	struct block_t *first_block = page->page_area;
	struct block_t *curr_block = first_block;
	for (int i = 0; i < num_blocks; i++)
	{
		// next block should be block_size + next pointer away from current block
		// TODO assert that pointer to every block + blocksize <= capacity
		struct block_t *tmp = (struct block_t *) ((char *) curr_block + page->block_size);
		tmp->next = NULL;
		curr_block->next = tmp;
		curr_block = tmp;

		// DEBUG
		assert(page->capacity >= (char *)tmp);
		
	}
	return first_block;
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
	enum page_kind_enum page_kind = get_page_kind(size);
	new_seg->page_kind = page_kind;

	// (From paper)...
	// we can calculate the page index by taking the difference and shifting by the
	// segment page_shift: for small pages this is 16 (= 64KiB), while for large and
	// huge pages it is 22 (= 4MiB) such that the index is always zero in those cases
	// (as there is just one page)

	new_seg->page_shift = page_kind == SMALL ? 16 : 22;
	new_seg->num_used_pages = 0;
	new_seg->num_free_pages = page_kind == SMALL ? NUM_PAGES_SMALL_SEGMENT : 1; // 64 pages in small, 1 in others
	new_seg->total_num_pages = page_kind == SMALL ? NUM_PAGES_SMALL_SEGMENT : 1;

	// pointer to start of pages array
	// new_seg->pages = new_seg + sizeof(segment); // TODO add padding
	new_seg->free_pages = &new_seg->pages[0];		// TODO: initialize all pages to be a ptr to next page

	//TODO: set each page to point to next one in array (pages)

	// TODO: DETERMINE PAGE AREA POINTER BASED ON SIZEOF METADATA
	// for all pages, multiply page metadata by # num pages and add size of segment metadata
	// set the page area ptr to this value
	// make sure it doesnt have some weird alignment
	// make sure the area_ptr + #num_pages * area_size <= segment_ptr + 4mb, make sure stuff is within segment

	return new_seg;
}

// create page
page *malloc_page(thread_heap *heap, size_t size, size_t page_num)
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
	page->block_size = nearest_block_size(size); //TODO: make an array of ALL block pages_sizes possible

	page->num_used = 0;

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
	
	// 
	// size_t usable_page_area = num_blocks * page->block_size;

	// size_t num_blocks;  
	
	// page area for this specific page
	// todo fix this pointer arithmetic
	size_t bytes_per_page = segment->page_kind == SMALL ? 64 * KB : 4 * MB; 
	size_t words_per_page = bytes_per_page/8; // 8 bytes per word

	size_t usable_page_area;
	if (page_num == 1)
	{
		page->page_area = (page_area *) segment + sizeof(struct segment);
		usable_page_area = bytes_per_page - sizeof(struct segment);
		// page->total_num_blocks = usable_page_area 
		// segment start - page_area 
		// page->reserved = segment + words_per_page;

	} else{
		// case of 64 pages
		page->page_area = (page_area *) segment + words_per_page * (page_num - 1);
		// page->reserved = page->page_area + words_per_page;
		usable_page_area = bytes_per_page;
	}
	
	// todo make sure this is valid.
	// uint64_t available_space = ((uint64_t)page->reserved - (uint64_t)page->page_area);
	page->total_num_blocks = usable_page_area/page->block_size;

	// These segments are 4MiB
	// (or larger for huge objects that are over 512KiB), and start with the segment- and
	// page meta data, followed by the actual pages where the first page is shortened
	// by the size of the meta data plus a guard page
 
	// TODO check that this is doing pointer arithmetic properly
	// should account for rounding.
	// page->capacity = page->page_area + (num_blocks * page->block_size);
	size_t number_of_bytes_used =  (page->total_num_blocks * page->block_size);
	page->capacity = ((char *)page->page_area) + number_of_bytes_used;

	// set up block_t freelist
	// page-> free is start of freelist
	page->free = create_free_blocks(page, page->total_num_blocks);

	// debug

	assert((char *) page->free == (char *)page->page_area);
	assert((char *) page->free < page->capacity);
	// debug, turn off via compile flag or something later
	// page->capacity = 
	if (page_num == 1)
	{
		char * segment_end = ((char *) segment) + 4 * MB;
		page->reserved = segment_end;
		assert(page->reserved >= page->capacity);
	}
	assert(page->capacity - (char *) segment < 4 * MB);

	return page;
	
}

// TODO: rotate linked list to optimize walking through the list
void *malloc_generic(thread_heap *heap, size_t size)
{
	size_t block_size = nearest_block_size(size);
	int64_t pages_direct_idx = pages_direct_index(size);
	
	for (struct page_reference *pg = heap->pages[size_class(size)]; pg != NULL; pg = pg->next) {
		page *page = pg->page;
		page_collect(page);
		if (page->num_used - page->num_thread_freed == 0) { // objects currently used - objects freed by other threads = 0
			page_free(page); // add page to segment's free_pages
		} else if (page->free != NULL && page->block_size == block_size) {
			// TODO: update pages direct
			// next free direct page

			if (pages_direct_idx >= 0) {//update pages_direct entry if block size is appropriate
				heap->pages_direct[pages_direct_idx] = page;
			}
			//TODO: When a page is found with free space, the page list is also rotated at that point so that a next search starts from that point.

			
			return mm_malloc(size);
		}


	}

	// page/segment alloc path.
	page * page = malloc_page(heap, size, 1);
	struct block_t * block = page->free;
	page->free =  block->next;

	return block;
}

void *malloc_large(thread_heap *heap, size_t size)
{
	size_t block_size = nearest_block_size(size);
	for (struct page_reference *pg = heap->pages[size_class(size)]; pg != NULL; pg = pg->next) {
		page *page = pg->page;
		if (page->free != NULL && page->block_size == block_size) {
			struct block_t* block = page->free;
			page->free = block->next;
			page->num_used++;
			return block;
		}
	}
	return malloc_generic(heap, size);
}

void *malloc_small(thread_heap *heap, size_t size)
{
	struct page* page = heap->pages_direct[(size + 7)>>3];
	struct block_t* block = page->free;
	if (block == NULL)
	{
		return malloc_generic(heap, size);
	}
	page->free = block->next;
	page->num_used++;
	return block;
}

size_t get_cpuid()
{
	return 1;
}

void *mm_malloc(size_t sz)
{
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
	} else if (sz <= 512 * KB)
	{
		return malloc_large(&tlb[cpu_id], sz);
	}
	return malloc_generic(&tlb[cpu_id], sz);

	// return NULL;
}

void mm_free(void *ptr)
{
	(void)ptr; /* Avoid warning about unused variable */
	// free(ptr);
	struct segment* segment = (struct segment *)((uintptr_t)ptr & ~(4*MB));
	if (segment == NULL)
	{
		return;
	}

	size_t page_index = ((uintptr_t)ptr - (uintptr_t)segment) >> segment->page_shift;
	assert(page_index == 0);//TODO: adjust for small pages or remove if not easy to do that
	
	page* page = &segment->pages[page_index];
	
	struct block_t* block = (struct block_t*)ptr;

	if (get_cpuid() == segment->cpu_id) { // local free
		block->next = page->local_free;
		page->local_free = block;
		page->num_used--;
		if (page->num_used - page->num_thread_freed == 0) page_free(page);
	} else { // non-local free TODO: finish this, make fields that need to be atomic actually atomic
		// atomic_push( &page->thread_free, block);
		// atomic_incr( &page->num_thread_freed );
	}
}



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
