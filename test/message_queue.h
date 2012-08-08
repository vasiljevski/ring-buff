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

#ifndef MESSAGE_QUEUE_H_
#define MESSAGE_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Message queue error codes.
 */
typedef enum
{
	MSG_QUEUE_ERR_OK,     /**< MSG_QUEUE_ERR_OK No error. */
	MSG_QUEUE_ERR_GENERAL,/**< MSG_QUEUE_ERR_GENERAL General error. */
	MSG_QUEUE_ERR_NO_MEM  /**< MSG_QUEUE_ERR_NO_MEM Not enough system memory error. */
} msg_queue_err_t;

/**
 * Message queue handle.
 */
typedef void* msg_queue_hndl;

/**
 * Message queue constructor.
 * @param handle pointer to handle which will be updated if construction was successful (output param)
 * @return error descriptor.
 */
msg_queue_err_t msg_queue_create(msg_queue_hndl *handle);
/**
 * Message queue destructor.
 * @param handle handle which will be freed.
 * @return error descriptor.
 */
msg_queue_err_t msg_queue_destroy(msg_queue_hndl handle);
/**
 * Put message in the queue.
 * @param handle message queue handle
 * @param msg_data message data. Data is preallocated by user (queue is not doing any data memory management).
 * @param msg_size message data size in bytes.
 * @return error descriptor.
 */
msg_queue_err_t msg_queue_put(msg_queue_hndl handle, void* msg_data, size_t msg_size);
/**
 * Put message as first one in the queue.
 * @param handle message queue handle.
 * @param msg_data message data. Data is preallocated by user (queue is not doing any data memory management).
 * @param msg_size message data size in bytes.
 * @return error descriptor.
 */
msg_queue_err_t msg_queue_put_urgent(msg_queue_hndl handle, void* msg_data, size_t msg_size);
/**
 * Get message from queue. Message related memory is freed, but message data needs to be freed by user.
 * This function will block execution until message is available in the queue.
 * @param handle message queue handle.
 * @param msg_data message data. This is output parameter which will contain data pointer if there were no errors.
 * @param msg_size message size. This is output parameter which will contain data size if there were no errors.
 * @return error descriptor.
 */
msg_queue_err_t msg_queue_get(msg_queue_hndl handle, void** msg_data, size_t* msg_size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MESSAGE_QUEUE_H_ */
