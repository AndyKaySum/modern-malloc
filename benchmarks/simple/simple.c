////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////////

/**
 * @file simple.c
 *
 * Simple unit test.
 */

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "mm_thread.h"
#include "timer.h"
#include "malloc.h"
#include "memlib.h"

int niterations = 1; // Default number of iterations.
int nobjects = 2048; // Default number of objects.
int nthreads = 1;	 // Default number of threads.
int work = 0;		 // Default number of loop iterations.
int size = 1;		 // base struct foo is 8 bytes.

struct Foo
{
	int x;
	int y;
};

extern void *worker(void *arg)
{
	int i, j;
	volatile int d;
	struct Foo **a;
	struct Foo ***nested;
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
	int cpu = (int)arg; // cpu number will fit in an int, ignore warning
#pragma GCC diagnostic pop

	setCPU(cpu);

	// alloc small object bigger than size*sizeof(struct foo) to ensure "fresh" page area

	// struct Foo *page_override = (struct Foo *)mm_malloc(8 * sizeof(struct Foo));
	nested = (struct Foo ***)mm_malloc(niterations * sizeof(struct Foo **));
	// now should be allocating to fresh page for 8byte objects
	for (j = 0; j < niterations; j++)
	{

		nested[j] = (struct Foo **)mm_malloc((nobjects / nthreads) * sizeof(struct Foo *));

		// printf ("a %d\n", j);
		for (i = 0; i < (nobjects / nthreads); i++)
		{
			printf ("malloc iteration %d object %d\n", j, i);
			nested[j][i] = (struct Foo *)mm_malloc(sizeof(struct Foo *)); // 8 bytes.
			// for (d = 0; d < work; d++) {
			// 	volatile int f = 1;
			// 	f = f + f;
			// 	f = f * f;
			// 	f = f + f;
			// 	f = f * f;
			// }
			*(nested[j][i]) = (struct Foo){i, i};
			assert(nested[j][i]->x == i && nested[j][i]->y == i);
		}

		// a[0] = (struct Foo *)mm_malloc(2048 * sizeof(struct Foo)); // 8 bytes.
			
	}
	// struct Foo *b = (struct Foo *)mm_malloc(16 * sizeof(struct Foo));
	// struct Foo *c = (struct Foo *)mm_malloc(16376);
	// struct Foo *testv = (struct Foo *)mm_malloc(4 * sizeof(struct Foo)); // 8 bytes.
	// struct Foo *z = (struct Foo *)mm_malloc(16380);

	for (j = 0; j < niterations; j++)
	{
		// printf ("a %d\n", j);
		for (i = 0; i < (nobjects / nthreads); i++)
		{
			// [i] = (struct Foo *)mm_malloc(8); // 8 bytes.
			// for (d = 0; d < work; d++) {
			// 	volatile int f = 1;
			// 	f = f + f;
			// 	f = f * f;
			// 	f = f + f;
			// 	f = f * f;
			// }
			assert(nested[j][i]->x == i && nested[j][i]->y == i);
		}

		// a[0] = (struct Foo *)mm_malloc(2048 * sizeof(struct Foo)); // 8 bytes.
	}

	for (j = 0; j < niterations; j++)
	{
		for (i = 0; i < (nobjects / nthreads); i++)
		{
			// [i] = (struct Foo *)mm_malloc(8); // 8 bytes.
			// for (d = 0; d < work; d++) {
			// 	volatile int f = 1;
			// 	f = f + f;
			// 	f = f * f;
			// 	f = f + f;
			// 	f = f * f;
			// }
			assert(nested[j][i]->x == i && nested[j][i]->y == i);
			printf ("freeing iteration %d object %d\n", j, i);
			mm_free(nested[j][i]);
		}

		// a[0] = (struct Foo *)mm_malloc(2048 * sizeof(struct Foo)); // 8 bytes.
		mm_free(nested[j]);
	}

	mm_free(nested);

	return NULL;
}

int main(int argc, char *argv[])
{
	struct timespec start_time;
	struct timespec end_time;
	int i;

	if (argc >= 2)
	{
		nthreads = atoi(argv[1]);
	}

	if (argc >= 3)
	{
		niterations = atoi(argv[2]);
	}

	if (argc >= 4)
	{
		nobjects = atoi(argv[3]);
	}

	if (argc >= 5)
	{
		work = atoi(argv[4]);
	}

	if (argc >= 6)
	{
		size = atoi(argv[5]);
	}

	/* Call allocator-specific initialization function */
	mm_init();

	//SMALL TEST: malloc, then free, should free page for reuse by next malloc
	// 			when we have segment free, the entire segment should be freed
	// long *small = mm_malloc(8);
	// mm_free(small);
	// long *bigger = mm_malloc(32);
	// mm_free(bigger);

	// // large segment
	// long *even_bigger = mm_malloc(16376);
	// mm_free(even_bigger);
	// long *even_bigger1 = mm_malloc(16376);
	// mm_free(even_bigger1);
	// return;
	// malloc of size 8
	pthread_t *threads = (pthread_t *)mm_malloc(nthreads * sizeof(pthread_t));
	int numCPU = getNumProcessors();

	pthread_attr_t attr;
	initialize_pthread_attr(PTHREAD_CREATE_JOINABLE, SCHED_RR, -10,
							PTHREAD_EXPLICIT_SCHED, PTHREAD_SCOPE_SYSTEM, &attr);

	printf("Running simple test for %d threads, %d iterations, %d objects, %d work and %d size...\n", nthreads, niterations, nobjects, work, size);

	/* Get the starting time */
	clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

	for (i = 0; i < nthreads; i++)
	{
		pthread_create(&threads[i], &attr, &worker, (void *)((u_int64_t)(i + 1) % numCPU));
	}

	for (i = 0; i < nthreads; i++)
	{
		pthread_join(threads[i], NULL);
	}

	/* Get the finish time */
	clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

	double t = timespec_diff(&start_time, &end_time);

	printf("Time elapsed = %f seconds\n", t);
	printf("Memory used = %ld bytes\n", mem_usage());

	mm_free(threads);

	return 0;
}
