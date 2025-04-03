# Modern Malloc (A2Alloc)

A high-performance memory allocator based on MiMalloc's free list sharding approach.

## Overview

This project implements a thread-efficient memory allocator for multi-core systems that avoids common performance pitfalls like false sharing and lock contention. The implementation is inspired by MiMalloc's approach of using free list sharding to increase locality, avoid contention, and support a highly-tuned allocate and free fast path.

## Design

### Allocator Choice

When searching the memory allocator literature, we considered multiple designs based on:
- Speed
- Scalability
- False sharing avoidance
- Low fragmentation
- Implementation complexity
- Code size

After evaluating options like SuperMalloc and MiMalloc, we chose to implement key features from MiMalloc due to its:
- Strong single-threaded performance
- Effective handling of false sharing
- Smaller code size through high code reuse
- Relatively simple core ideas to manage concurrency

### Core Idea: Free List Sharding

The primary concept used in this allocator is free list sharding. Each page (minimum size 64KiB) has multiple free lists:

- **Local free list**: Used when the thread that "owns" the page frees a block
- **Thread-free list**: Used when a non-owner thread frees a block (uses atomic operations)
- **Page free list**: Main free list that only gets updated when necessary

This approach reduces the number of atomic operations needed for synchronization by deferring maintenance tasks.

### Implementation Details

#### Core Data Structures

- **Blocks**: Fixed-size memory areas assigned to objects
- **Pages**: Similar to superblocks in Hoard, these store equal-sized blocks and come in three kinds:
  - Small (64KiB): Stores blocks from 8 bytes to 1024 bytes
  - Large: Stores blocks between 1024 bytes and 512KiB
  - Huge: Stores blocks larger than 512KiB
- **Segments**: 4MB aligned regions that store pages of the same kind
  - Small segments store 64 small pages
  - Large/huge pages occupy one or more entire segments

#### Allocation Strategy

1. Get CPU ID to obtain a thread-local heap
2. For small blocks (≤1024 bytes), use direct pointers to pages
3. For larger blocks, use linked lists of pages organized by power-of-2 size classes
4. Huge allocations span entire segments

#### Memory Reuse

When freeing memory:
1. Blocks are added to the appropriate free list
2. Page cleanup is deferred until no free references are available
3. Pages are marked for reuse when all blocks are freed
4. Segments are marked for reuse when all pages are freed
5. A bitmap tracks segment allocation status with mutex protection

## Performance

### False Sharing Avoidance

- **Passive false sharing**: Avoided by ensuring that only the owner thread can reallocate memory while a page is in use
- **Active false sharing**: Eliminated by making each page owned by a specific thread

### Scalability

Our implementation achieves better runtime at all thread counts by:
- Increasing the locality of free lists
- Reducing concurrent access to shared data
- Deferring atomic operations until necessary

### Sequential Speed

A2Alloc outperforms both kheap and libc in sequential speed by deferring expensive tasks until the free list of blocks is empty.

### Memory Usage

A2Alloc uses more memory than kheap or libc due to:
- Overhead from free list sharding
- Additional heap metadata for page lists and direct pointers
- 4MB segment allocation granularity

This increased memory usage is a tradeoff for better performance, as tests with smaller segment sizes showed decreased performance.

## References

1. E. D. Berger, K. S. McKinley, R. D. Blumofe, and P. R. Wilson, "Hoard: A scalable memory allocator for multithreaded applications," ACM Sigplan Notices, vol. 35, no. 11, pp. 117–128, 2000.
2. B. C. Kuszmaul, "Supermalloc: A super fast multithreaded malloc for 64-bit machines," in Proceedings of the 2015 International Symposium on Memory Management, 2015, pp. 41–55.
3. D. Leijen, B. Zorn, and L. de Moura, "Mimalloc: Free list sharding in action," in Programming Languages and Systems: 17th Asian Symposium, APLAS 2019, Nusa Dua, Bali, Indonesia, December 1–4, 2019, Proceedings 17. Springer, 2019, pp. 244–265. 