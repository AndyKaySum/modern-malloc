#include <stdlib.h>
#include <stdint.h>
#include "memlib.h"

//NOTE: copied from kheap
#define PAGE_SIZE  4096
#if defined(__GNUC__)
#if __LP64__
#define PAGE_FRAME 0xfffffffffffff000
#else
#define PAGE_FRAME 0xfffff000
#endif /* __X86_64__ */
#endif /* __GNUC__ */

#define NSIZES 9
static const size_t sizes[NSIZES] = { 8, 16, 32, 64, 128, 256, 512, 1024, 2048 };

#define SMALLEST_SUBPAGE_SIZE 8
#define LARGEST_SUBPAGE_SIZE 2048

typedef ptrdiff_t vaddr_t;
////////////////////////////////////////

#define CACHESIZE 128 /* Should cover most machines. */ //NOTE: copied from Ex2
#define MB 1048576 //2^20 bytes
#define KB 1024 //2^10 bytes

typedef uint64_t thread_id;

struct freelist {
	struct freelist *next;
};

struct pageref {
	struct pageref *next;
	struct freelist *flist;
	// vaddr_t pageaddr_and_blocktype;
	int nfree;
};

struct big_freelist {
	int npages;
	struct big_freelist *next;
};

struct page {
	struct freelist *free;
	struct freelist *local_free;
	struct freelist *thread_free;
	size_t used;
	size_t thread_freed; // Number of blocks freed by other threads
	size_t capacity;
	size_t reserved;
	vaddr_t pageaddr_and_blocktype; 
} typedef page;

enum page_type {
	SMALL, //64KB 8-1024 bytes
	LARGE, //objects under 512KB, 1 large page that spans whole segment
	HUGE, // objects over 512KB
};
struct segment {
	thread_id thread_id;
	uint32_t page_shift;//for small pages this is 16 (= 64KiB), while for large and huge pages it is 22 (= 4MiB) such that the index is always zero in those cases (as there is just one page)
	enum page_type type;
	size_t used;
	size_t free;
	page *pages; // 1 page if large or huge, 64 equal-sized pages if small
} __attribute__ ((aligned (4*MB))) /*NOTE: this only works on gcc*/ typedef segment; 

struct thread_heap {
	page *pages_direct;
	page *pages;
	thread_id thread_id; // CPU number
	page *small_page_refs; //freelist of unallocated small page refs
	segment *free_segment_refs; // linked list of freed segments that can be written to
	// check before allocating new segment with mem_sbrk
	uint8_t padding[CACHESIZE-40];//ensure that false sharing does not occur for this
} typedef thread_heap;


struct main_heap {
	thread_heap *thread_heaps;
};

void init_thread_heap(thread_id id) {
	
}

thread_heap *get_heap(thread_id id) {

}

void *malloc_generic(size_t size) {
	
}

void *malloc_small(size_t size) {
	return malloc_generic(size);
}

void *mm_malloc(size_t sz)
{
	(void)sz; /* Avoid warning about unused variable */
	return malloc(sz);

	if (sz <= 8*KB) {
		return malloc_small(sz);
	}
	return malloc_generic(sz);

	// return NULL;
}

void mm_free(void *ptr)
{
	(void)ptr; /* Avoid warning about unused variable */
	free(ptr);
}


int mm_init(void)
{
	if (dseg_lo == NULL && dseg_hi == NULL) {
		return mem_init();
	}
	return 0;
}

