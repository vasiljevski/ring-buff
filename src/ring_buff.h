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


#ifndef RING_BUFF_H_
#define RING_BUFF_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Error types.
 */
typedef enum ring_buff_err
{
	/** No error */
	RING_BUFF_ERR_OK = 0,
	/** General error */
	RING_BUFF_ERR_GENERAL,
	/** Out of memory error */
	RING_BUFF_ERR_NO_MEM,
	/** Buffer overrun error */
	RING_BUFF_ERR_OVERRUN,
	/** Request size wrong error */
	RING_BUFF_ERR_SIZE,
	/** Internal (system) error */
	RING_BUFF_ERR_INTERNAL,
	/** Operation not permitted (e.g. reading after cancel is called) */
	RING_BUFF_ERR_PERM
} ring_buff_err_t;

/** Ring buffer handle. */
typedef void* ring_buff_handle_t;
/**
 * Notification function type. Ring buffer client has to implement one,
 * if notification mechanism is used.
 * @param handle Ring buffer handle
 * @param buff Notified data buffer
 * @param buff Notified data buffer size
 * @return RING_BUFF_ERR_OK if everything was OK, or error if ring buffer client detected some problem
 */
typedef ring_buff_err_t (*ring_buff_notify_t) (ring_buff_handle_t handle, void* buff, uint32_t size);

/**
 * Ring buffer attribute structure. It is used when ring buffer is created.
 */
typedef struct ring_buff_attr
{
	/** Memory used for ring buffer. */
	void* buff;
	/** Buffer size. */
	uint32_t size;
	/**
	 * Accumulate size. If set to 0, accumulation/notification mechanism will not be used.
	 */
	uint32_t accumulate;
	/**
	 * Notify function pointer. This function is called whenever end of buffer is reached,
	 * or there is up to "accumulate" number of bytes available. If "accumulate" attribute
	 * field is not set, this function will NOT be called.
	 * NOTE: This function is called from the same thread from which data commit is done.
	 */
	ring_buff_notify_t notify_func;
} ring_buff_attr_t;

/**
 * Creates ring buffer with attributes passed as argument. Handle returned must be saved
 * by ring buffer client, so that it can be used for operations on ring buffer.
 * @param attr Ring buffer attribute object.
 * @param handle Pointer to the handle. This argument must not be NULL. If function returns
 * without error, this pointer will point to ring buffer handle that is required for other
 * ring buffer operations.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_create(ring_buff_attr_t* attr, ring_buff_handle_t* handle);
/**
 * Ring buffer destructor function. This function must be called, so that all resources
 * allocated on ring buffer construction are freed.
 * @param handle Ring buffer handle.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_destroy(ring_buff_handle_t handle);
/**
 * Reserves chunk of memory from ring buffer. Chunk allocated is continuous memory that
 * is ready to be written with data.
 * @param handle Ring buffer handle.
 * @param buff Pointer to the reserved data. This is output value.
 * @param size Requested buffer size in bytes.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_reserve(ring_buff_handle_t handle, void** buff, uint32_t size);
/**
 * Commits written data. After this function is called, data is available for reading.
 * @param handle Ring buffer handle.
 * @param buff Pointer to data that should be committed. This pointer is retrieved with "ring_buff_reserve".
 * @param size Committed data size in bytes.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_commit(ring_buff_handle_t handle, void* buff, uint32_t size);
/**
 * Frees ring buffer chunk, so that it can be used for writing.
 * @param handle Ring buffer handle.
 * @param buff Pointer to data that should be freed.
 * @param size Chunk length that should be freed.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_free(ring_buff_handle_t handle, void* buff, uint32_t size);
/**
 * Reads out requested size of data. Data should be additionally freed with "ring_buff_free".
 * Read mechanism should NOT be used together with the notify mechanism.
 * @param handle Ring buffer handle.
 * @param buff Output argument that will contain pointer with read data.
 * @param size Data size that should be read.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_read(ring_buff_handle_t handle, void** buff, uint32_t size, uint32_t *read);
/**
 * This function can be used when the notify mechanism is used. Calling this function will result
 * with forced call to the notify function.
 * @param handle Ring buffer handle.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_flush(ring_buff_handle_t handle);
/**
 * This function will stop further operations on ring buffer.
 * It will NOT free any resources.
 * @param handle Ring buffer handle.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_cancel(ring_buff_handle_t handle);
/**
 * Similar to ring_buff_cancel, but reader can still read rest of the data provided.
 * It should be called after the last 'commit' is done.
 * @param handle Ring buffer handle.
 * @return RING_BUFF_ERR_OK if everything was OK, or error if there was some problem.
 */
ring_buff_err_t ring_buff_stop(ring_buff_handle_t handle);
/**
 * Convenience function that prints out 'human readable' ring buffer error description.
 * @param err Error.
 */
void ring_buff_print_err(ring_buff_err_t err);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RING_BUFF_H_ */
