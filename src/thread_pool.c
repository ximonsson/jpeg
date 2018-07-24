#include "thread_pool.h"
#include <string.h>
#include <stdlib.h>


#define THREAD_POOL_SIZE 10


int thread_pool_init (thread_pool* pool)
{
    pool->size      = 0;
    pool->__front__ =
    pool->__back__  =
    pool->arguments = malloc (THREAD_POOL_SIZE * sizeof (thread_pool));
    if (!pool->arguments)
        return 1;

    memset (pool->arguments, 0, THREAD_POOL_SIZE * sizeof (thread_pool));
    pthread_mutex_init (&pool->mutex,          NULL);
    pthread_cond_init  (&pool->push_condition, NULL);
    pthread_cond_init  (&pool->pop_condition,  NULL);
    return 0;
}


void thread_pool_destroy (thread_pool* pool)
{
    pthread_cond_broadcast (&pool->push_condition);
    pthread_cond_broadcast (&pool->pop_condition);
    free (pool->arguments);
    pthread_mutex_destroy (&pool->mutex);
    pthread_cond_destroy  (&pool->push_condition);
    pthread_cond_destroy  (&pool->pop_condition);
    pool->size      = 0;
    pool->__front__ =
    pool->__back__  =
    pool->arguments = NULL;
}


int thread_pool_pop (thread_pool* pool, thread_args* args)
{
	int ret = 0;
    pthread_mutex_lock (&pool->mutex);
	if (pool->size == 0)
	{
        pthread_cond_wait (&pool->push_condition, &pool->mutex);
        if (pool->size == 0)
        {
            ret = THREAD_POOL_EMPTY;
            goto end;
        }
	}

	*args = *pool->__front__;
	pool->__front__ ++;
	pool->size --;
	if (pool->__front__ - pool->arguments == THREAD_POOL_SIZE)
		pool->__front__ = pool->arguments;

    pthread_cond_signal (&pool->pop_condition);
end:
	pthread_mutex_unlock (&pool->mutex);
	return ret;
}


int thread_pool_push (thread_pool* pool, uint8_t* source, uint8_t* destination, int offset, uint8_t step, int bitp)
{
    int ret = 0;
    pthread_mutex_lock (&pool->mutex);
    // check size
	if (pool->size == THREAD_POOL_SIZE)
	{
        pthread_cond_wait (&pool->pop_condition, &pool->mutex);
        if (pool->size == THREAD_POOL_SIZE)
        {
            ret = THREAD_POOL_FULL;
            goto end;
        }
	}
    // push to back
	pool->__back__->source      = source;
    pool->__back__->destination = destination;
    pool->__back__->offset      = offset;
    pool->__back__->step        = step;
    pool->__back__->bitp        = bitp;
	pool->__back__ ++;
	pool->size ++;

	if (pool->__back__ - pool->arguments == THREAD_POOL_SIZE)
		pool->__back__ = pool->arguments;

    pthread_cond_signal (&pool->push_condition);
end:
	pthread_mutex_unlock (&pool->mutex);
	return ret;
}
