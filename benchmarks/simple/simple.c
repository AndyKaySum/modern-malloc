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

int niterations = 1;	// Default number of iterations.
int nobjects = 2048;   // Default number of objects.
int nthreads = 1;	// Default number of threads.
int work = 0;		// Default number of loop iterations.
int size = 1; // base struct foo is 8 bytes.

struct Foo {
  int x;
  int y;
};


extern void * worker (void *arg)
{
  int i, j;
  volatile int d;
  struct Foo ** a;
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
  int cpu = (int)arg; // cpu number will fit in an int, ignore warning
#pragma GCC diagnostic pop

  setCPU(cpu);


  a = (struct Foo **)mm_malloc((nobjects / nthreads) * sizeof(struct Foo *));

// alloc small object bigger than size*sizeof(struct foo) to ensure "fresh" page area

struct Foo * page_override = (struct Foo *)mm_malloc(8*sizeof(struct Foo));

// now should be allocating to fresh page for 8byte objects
  for (j = 0; j < niterations; j++) {

    //printf ("a %d\n", j);
    for (i = 0; i < (nobjects / nthreads); i ++) {
      a[i] = (struct Foo *)mm_malloc(1*sizeof(struct Foo)); // 8 bytes.
    //   for (d = 0; d < work; d++) {
	// volatile int f = 1;
	// f = f + f;
	// f = f * f;
	// f = f + f;
	// f = f * f;
    //   }
      assert (a[i]);
    }
    
	// free objects
    // for (i = 0; i < (nobjects / nthreads); i ++) {
    //   mm_free(a[i]);
    //   for (d = 0; d < work; d++) {
	// volatile int f = 1;
	// f = f + f;
	// f = f * f;
	// f = f + f;
	// f = f * f;
    //   }
    // }
  }

  struct Foo *b = (struct Foo *)mm_malloc(16*sizeof(struct Foo));
  struct Foo *c = (struct Foo *)mm_malloc(2048*sizeof(struct Foo));

  return NULL;
}


int main (int argc, char * argv[])
{
	struct timespec start_time;
	struct timespec end_time;
	int i;
	
	if (argc >= 2) {
		nthreads = atoi(argv[1]);
	}
	
	if (argc >= 3) {
		niterations = atoi(argv[2]);
	}
	
	if (argc >= 4) {
		nobjects = atoi(argv[3]);
	}
	
	if (argc >= 5) {
		work = atoi(argv[4]);
	}
	
	if (argc >= 6) {
		size = atoi(argv[5]);
	}


	/* Call allocator-specific initialization function */
	mm_init();
	
	pthread_t *threads = (pthread_t *)mm_malloc(nthreads*sizeof(pthread_t));
	int numCPU = getNumProcessors();

	pthread_attr_t attr;
	initialize_pthread_attr(PTHREAD_CREATE_JOINABLE, SCHED_RR, -10, 
				PTHREAD_EXPLICIT_SCHED, PTHREAD_SCOPE_SYSTEM, &attr);

	printf ("Running simple test for %d threads, %d iterations, %d objects, %d work and %d size...\n", nthreads, niterations, nobjects, work, size);

	/* Get the starting time */
	clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

	for (i = 0; i < nthreads; i++) {
		pthread_create(&threads[i], &attr, &worker, (void *)((u_int64_t)(i+1)%numCPU));
	}

	for (i = 0; i < nthreads; i++) {
		pthread_join(threads[i], NULL);
	}

	/* Get the finish time */
	clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

	double t = timespec_diff(&start_time, &end_time);

	printf ("Time elapsed = %f seconds\n", t);
	printf ("Memory used = %ld bytes\n",mem_usage());
	
	mm_free(threads);

	return 0;
}
