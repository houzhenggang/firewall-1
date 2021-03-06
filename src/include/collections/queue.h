#ifndef SQUEUE_H
#define SQUEUE_H

#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <stdatomic.h>
#include <string.h>
#include <stdint.h>
/*
	A simple wait-free single enqueuer - single dequeuer queue
	note: this cannot store more than `size` elements
*/
typedef struct squeue {
	int size;
	int head;
	int tail;
	pthread_cond_t cond;
	pthread_mutex_t m;
	void **buff;
} squeue_t;

squeue_t *squeue_setup(int size);
void squeue_tear_down(squeue_t *q);
void squeue_enq(squeue_t *q, void *x);
void *squeue_deq();

// waits
void *squeue_deq_wait(squeue_t *q);

#endif