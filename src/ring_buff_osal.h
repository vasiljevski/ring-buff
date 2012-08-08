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

#ifndef RING_BUFF_OS_ADAPT_H_
#define RING_BUFF_OS_ADAPT_H_

#include "ring_buff.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Mutex handle.
 */
typedef void* ring_buff_mutex_t;

/**
 * Creates new mutex. It is up to client to keep the handle. Mutex in <b>NOT</b> reentrant.
 * @param handle Pointer to the handle. This argument must not be NULL. If function returns
 * without error, this pointer will point to mutex handle which is required for other
 * mutex operations.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_mutex_create(ring_buff_mutex_t *handle);
/**
 * Mutex destructor function. This function must be called, so that all resources
 * allocated on mutex construction are freed.
 * @param handle Mutex handle.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_mutex_destroy(ring_buff_mutex_t handle);
/**
 * Locks mutex.
 * @param handle Mutex handle.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_mutex_lock(ring_buff_mutex_t handle);
/**
 * Unlocks mutex.
 * @param handle Mutex handle.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_mutex_unlock(ring_buff_mutex_t handle);


/**
 * Binary semaphore handle.
 */
typedef void* ring_buff_binary_sem_t;


/**
 * Creates new binary semaphore. It is up to client to keep the handle.
 * @param handle Pointer to the handle. This argument must not be NULL. If function returns
 * without error, this pointer will point to binary semaphore handle which is required for other
 * binary semaphore operations.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_binary_sem_create(ring_buff_binary_sem_t *handle);
/**
 * Binary semaphore destructor function. This function must be called, so that all resources
 * allocated on binary semaphore construction are freed.
 * @param handle Binary semaphore.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_binary_sem_destroy(ring_buff_binary_sem_t handle);
/**
 * Takes binary semaphore.
 * @param handle Binary semaphore handle.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_binary_sem_take(ring_buff_binary_sem_t handle);
/**
 * Gives binary semaphore.
 * @param handle Binary semaphore handle.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_binary_sem_give(ring_buff_binary_sem_t handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RING_BUFF_OS_ADAPT_H_ */
