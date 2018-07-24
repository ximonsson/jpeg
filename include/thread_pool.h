/** ------------------------------------------------------------------------------------
 *  File: thread_pool.h
 *  Description: API towards threads safe buffer.
 *  ------------------------------------------------------------------------------------ */
#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include <pthread.h>
#include <stdint.h>

#define THREAD_POOL_EMPTY   1
#define THREAD_POOL_FULL    2

/**
 *  Arguments for a thread.
 *  Kind of only usable for decoding/encoding threads containing a
 *  pointer to source data and destination buffer.
 */
typedef struct
{
    // source data
    uint8_t*    source;
    // destination buffer
    uint8_t*    destination;
    // offset in destination buffer
    int         offset;
    // step between values in buffer.
    // this value should be set to logn(step)
    uint8_t     step;
    // bit pointer to start from
    int         bitp;
}
thread_args;

/**
 *  A thread safe pool of arguments for threads.
 *  Implemented a FIFO queue.
 */
typedef struct
{
    size_t          size;
    pthread_mutex_t mutex;
    pthread_cond_t  push_condition;
    pthread_cond_t  pop_condition;
    thread_args*    arguments;
    thread_args*    __front__;
    thread_args*    __back__;
}
thread_pool;

/**
 *  Initialize a new thread pool.
 *  Returns non-zero on failure.
 */
int thread_pool_init (thread_pool* /* buffer */);

/**
 *  Destroy and deallocate a previously created thread pool.
 */
void thread_pool_destroy (thread_pool* /* buffer */);

/**
 *  Pop new arguments from the top of the pool.
 *  Returns zero on success else THREAD_POOL_EMPTY.
 */
int thread_pool_pop (thread_pool* /* buffer */, thread_args* /* args */);

/**
 *  Push new arguments to the back of the pool.
 *  Returns zero on success else THREAD_POOL_FULL.
 */
int thread_pool_push (thread_pool*  /* pool */,
                      uint8_t*      /* source */,
                      uint8_t*      /* destination */,
                      int           /* offset */,
                      uint8_t       /* step */,
                      int           /* bitp */);

#endif
