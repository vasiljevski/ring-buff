/*******************************************************************************
 *
 * Copyright (c) 2012 Vladimir Maksovic
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * * Neither Vladimir Maksovic nor the names of this software contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL VLADIMIR MAKSOVIC
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/
#include <stdlib.h>
#include <pthread.h>

#include "ring_buff_osal.h"

/* ############### Mutex implementation ################ */

#define CAST_TO_PTHREAD_MUTEX(handle) ((pthread_mutex_t*)handle)

ring_buff_err_t ring_buff_mutex_create(ring_buff_mutex_t *handle)
{
	pthread_mutex_t *mtx = (pthread_mutex_t *)malloc(sizeof (pthread_mutex_t));
	if(mtx == NULL)
	{
		*handle = NULL;
		return RING_BUFF_ERR_NO_MEM;
	}
	if(pthread_mutex_init(mtx, NULL))
	{
		free(mtx);
		return RING_BUFF_ERR_GENERAL;
	}
	*handle = mtx;
	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_mutex_destroy(ring_buff_mutex_t handle)
{
	pthread_mutex_t *mtx = CAST_TO_PTHREAD_MUTEX(handle);
	pthread_mutex_destroy(mtx);
	free(mtx);
	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_mutex_lock(ring_buff_mutex_t handle)
{
	pthread_mutex_t *mtx = CAST_TO_PTHREAD_MUTEX(handle);
	pthread_mutex_lock(mtx);
	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_mutex_unlock(ring_buff_mutex_t handle)
{
	pthread_mutex_t *mtx = CAST_TO_PTHREAD_MUTEX(handle);
	pthread_mutex_unlock(mtx);
	return RING_BUFF_ERR_OK;
}

/* ############### Binary semaphore implementation ################ */
/* It is based on great tutorial by Shun Yan Cheung on pthread and locking:
 * http://www.mathcs.emory.edu/~cheung/Courses/455/Syllabus/5c-pthreads/sync.html
 */

typedef struct _bin_sema
{
	/**
	 * Cond. variable - used to block threads
	 */
	pthread_cond_t cv;
	/**
	 * Mutex variable - used to prevents concurrent access to the variable "flag"
	 */
	pthread_mutex_t mutex;
	/**
	 * Semaphore state: 0 = down, 1 = up
	 */
	int flag;
} bin_sema_t;

#define CAST_TO_PTHREAD_BIN_SEMA(handle) ((bin_sema_t*)handle)

ring_buff_err_t ring_buff_binary_sem_create(ring_buff_binary_sem_t *handle)
{
	/* Allocate space for bin_sema */
	bin_sema_t *s = (bin_sema_t *) malloc(sizeof(bin_sema_t));
	if(s == NULL)
	{
		*handle = NULL;
		return RING_BUFF_ERR_NO_MEM;
	}
	/* Init mutex */
	pthread_mutex_init(&(s->mutex), NULL);
	/* Init cond. variable */
	pthread_cond_init(&(s->cv), NULL);
	/* Set flag value */
	s->flag = 1;
	*handle = s;
	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_binary_sem_destroy(ring_buff_binary_sem_t handle)
{
	bin_sema_t *s = CAST_TO_PTHREAD_BIN_SEMA(handle);
	pthread_mutex_destroy(&(s->mutex));
	pthread_cond_destroy(&(s->cv));
	free(s);

	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_binary_sem_take(ring_buff_binary_sem_t handle)
{
	bin_sema_t *s = CAST_TO_PTHREAD_BIN_SEMA(handle);

	/* Try to get exclusive access to s->flag */
	pthread_mutex_lock(&(s->mutex));
	/* Examine the flag and wait until flag == 1 */
	while (s->flag == 0)
	{
		pthread_cond_wait(&(s->cv), &(s->mutex));
	}
	/* Semaphore is successfully taken */
	s->flag = 0;
	/* Release exclusive access to s->flag */
	pthread_mutex_unlock(&(s->mutex));

	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_binary_sem_give(ring_buff_binary_sem_t handle)
{
	bin_sema_t *s = CAST_TO_PTHREAD_BIN_SEMA(handle);

	/* Try to get exclusive access to s->flag */
	pthread_mutex_lock(&(s->mutex));
	/* Signal those that are waiting */
	pthread_cond_signal(&(s->cv));
	/* Update semaphore state to Up */
	s->flag = 1;
	/* Release exclusive access to s->flag */
	pthread_mutex_unlock(&(s->mutex));

	return RING_BUFF_ERR_OK;
}
