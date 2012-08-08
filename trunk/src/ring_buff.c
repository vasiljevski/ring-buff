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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ring_buff.h"
#include "ring_buff_osal.h"

/* #define RING_BUFF_DBG_MSG */

#define GET_RING_BUFF_OBJ(handle) ((ring_buff_obj_t*)handle)
#define ENTER_RING_BUFF_CONTEX(handle) (ring_buff_mutex_lock(handle->lock))
#define LEAVE_RING_BUFF_CONTEX(handle) (ring_buff_mutex_unlock(handle->lock))

/**
 * Buffer states. Buffer can be ONLY in ONE of possible states, but states are
 * defined so that bitwise or is possible.
 */
typedef enum ring_buff_state
{
	RING_BUFF_STATE_ACTIVE = 1,  /**< Active (normal) state. */
	RING_BUFF_STATE_CANCELED = 2,/**< Buffering was canceled. */
	RING_BUFF_STATE_STOPPED = 4  /**< Buffer is stopped (e.g. end of stream). */
} ring_buff_state_t;

typedef struct ring_buff_obj
{
	/** Buffer */
	uint8_t *buff;
	/** Buffer size */
	uint32_t size;
	/** Accumulate window size */
	uint32_t accumulate;
	/** Notify callback, called whenever window is filled with data and available for consuming */
	ring_buff_notify_t notify_func;
	/** Read pointer (available data start) */
	uint8_t *read;
	/** Write pointer (free memory start) */
	uint8_t *write;
	/** Accumulation window pointer (it does not have info about wrapped data) */
	uint8_t *acc;
	/**
	 * End of Data pointer. Used in reading mode. It marks last byte available for
	 * reading before write pointer wrapped to the buffer start.
	 */
	uint8_t *eod;
	/** Continuous data available */
	uint32_t acc_size;
	/** State */
	ring_buff_state_t state;
	/** Buffer lock */
	ring_buff_mutex_t lock;
	/** Read semaphore */
	ring_buff_binary_sem_t read_sem;
	/** Write semaphore */
	ring_buff_binary_sem_t write_sem;
} ring_buff_obj_t;

ring_buff_err_t ring_buff_create(ring_buff_attr_t* attr, ring_buff_handle_t* handle)
{
	ring_buff_obj_t* obj = NULL;
	ring_buff_err_t err_code = RING_BUFF_ERR_GENERAL;

	if(attr == NULL)
	{
		goto done;
	}
	if(attr->size == 0 || attr->buff == NULL)
	{
		goto done;
	}
	obj = malloc(sizeof(ring_buff_obj_t));
	if(obj == NULL)
	{
		err_code = RING_BUFF_ERR_NO_MEM;
		goto done;
	}
	if(ring_buff_mutex_create(&(obj->lock)) != RING_BUFF_ERR_OK)
	{
		free(obj);
		obj = NULL;
		goto done;
	}
	if(attr->accumulate > (attr->size / 2))
	{
		fprintf(stderr, "WARNING (%s): Accumulation set too high. It will be turned OFF!\n", __FUNCTION__);
		obj->accumulate = 0;
	}
	else
	{
		obj->accumulate = attr->accumulate;
	}
	obj->buff = attr->buff;
	obj->size = attr->size;
	obj->notify_func = attr->notify_func;
	obj->read = attr->buff;
	obj->write = attr->buff;
	obj->acc = attr->buff;
	obj->eod = NULL;
	obj->acc_size = 0;
	obj->state = RING_BUFF_STATE_ACTIVE;
	ring_buff_binary_sem_create(&(obj->read_sem));
	ring_buff_binary_sem_create(&(obj->write_sem));
	err_code = RING_BUFF_ERR_OK;

done:
	*handle = obj;
	return err_code;
}

/**
 * Internal function that checks weather the buffer is in expected state.
 * It is internal function, and expects that buffer context is already acquired by the caller.
 * @param handle Valid buffer handle.
 * @param states States to check against (use bitwise or to pass more than one).
 * @return RING_BUFF_ERR_OK if buffer is in expected state, RING_BUFF_ERR_PERM otherwise
 */
static ring_buff_err_t ring_buff_check_state(ring_buff_obj_t* obj, ring_buff_state_t states)
{
	if((obj->state & states) == 0)
	{
		return RING_BUFF_ERR_PERM;
	}
	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_destroy(ring_buff_handle_t handle)
{
	ring_buff_obj_t* obj = GET_RING_BUFF_OBJ(handle);

	if(obj == NULL)
	{
		return RING_BUFF_ERR_GENERAL;
	}
	ring_buff_mutex_destroy(obj->lock);
	ring_buff_binary_sem_destroy(obj->read_sem);
	ring_buff_binary_sem_destroy(obj->write_sem);
	free(obj);

	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_reserve(ring_buff_handle_t handle, void** buff, uint32_t size)
{
	ring_buff_obj_t* obj = GET_RING_BUFF_OBJ(handle);

	if(size > obj->size)
	{
		return RING_BUFF_ERR_SIZE;
	}
	ENTER_RING_BUFF_CONTEX(obj);
	if(ring_buff_check_state(obj, RING_BUFF_STATE_ACTIVE))
	{
		LEAVE_RING_BUFF_CONTEX(obj);
		return RING_BUFF_ERR_PERM;
	}
	/* simple situation, there is enough space left till the end of buffer */
	if(obj->write + size <= obj->buff + obj->size)
	{
		/* don't want to overwrite read buffer partition, wait for free chunk if read is too close up-front */
		while(obj->write < obj->read && obj->write + size >= obj->read)
		{
			/* unlock context */
			LEAVE_RING_BUFF_CONTEX(obj);
			/* wait for some free chunk */
			ring_buff_binary_sem_take(obj->write_sem);
			ENTER_RING_BUFF_CONTEX(obj);
			if(ring_buff_check_state(obj, RING_BUFF_STATE_ACTIVE))
			{
				LEAVE_RING_BUFF_CONTEX(obj);
				return RING_BUFF_ERR_PERM;
			}
		}
		*buff = obj->write;
		obj->write += size;
	}
	/* wrap around */
	else
	{
#ifdef RING_BUFF_DBG_MSG
		printf("RESERVE: Wrap around %d (%p) RD %p ACC %p WR %p\n", size, obj->buff, obj->read, obj->acc, obj->write);
#endif
		/* try to get buffer from the beginning, and be sure that read is not overwritten */
		while((obj->read - size) < obj->buff || obj->read > obj->write)
		{
#ifdef RING_BUFF_DBG_MSG
			printf("RESERVE: Waiting start free buffer (%d) (%p) RD %p ACC %p WR %p \n", size, obj->buff, obj->read, obj->acc, obj->write);
#endif
			LEAVE_RING_BUFF_CONTEX(obj);
			ring_buff_binary_sem_take(obj->write_sem);
			ENTER_RING_BUFF_CONTEX(obj);
			if(ring_buff_check_state(obj, RING_BUFF_STATE_ACTIVE))
			{
				LEAVE_RING_BUFF_CONTEX(obj);
				return RING_BUFF_ERR_PERM;
			}
		}
		/* reader must not exceed data available (current write) */
		obj->eod = obj->write;
		*buff = obj->buff;
		obj->write = obj->buff + size;
	}
	LEAVE_RING_BUFF_CONTEX(obj);

	return RING_BUFF_ERR_OK;
}

static ring_buff_err_t ring_buff_handle_acc(ring_buff_obj_t* obj, uint32_t added_size)
{
	void* buff = NULL;
	uint32_t size;

	ENTER_RING_BUFF_CONTEX(obj);
	/* send notification if there is enough data accumulated, or we got to the end of buffer */
	if(obj->acc + obj->acc_size > obj->buff + obj->size)
	{
		size = obj->acc_size - added_size;
		buff = obj->acc;
		obj->acc_size = added_size;

		obj->acc = obj->buff;
	}
	else if(obj->acc_size >= obj->accumulate)
	{
		size = obj->acc_size - added_size;
		buff = obj->acc;
		obj->acc_size = added_size;
		obj->acc += size;
	}
	LEAVE_RING_BUFF_CONTEX(obj);
	if(buff)
	{
		return obj->notify_func(obj, buff, size);
	}
	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_commit(ring_buff_handle_t handle, void* buff, uint32_t size)
{
	ring_buff_obj_t* obj = GET_RING_BUFF_OBJ(handle);

	ENTER_RING_BUFF_CONTEX(obj);

	obj->acc_size += size;
	/* Sanity check. This may be removed. */
	if(obj->acc_size > obj->size)
	{
		LEAVE_RING_BUFF_CONTEX(obj);
		return RING_BUFF_ERR_GENERAL;
	}
	LEAVE_RING_BUFF_CONTEX(obj);
	/* in case of accumulation, leave buffer context and notify listener */
	if(obj->accumulate && obj->notify_func)
	{
		return ring_buff_handle_acc(obj, size);
	}
	/* Read functionality may be used only if we don't accumulate data */
	else
	{
		ring_buff_binary_sem_give(obj->read_sem);
	}

	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_free(ring_buff_handle_t handle, void* buff, uint32_t size)
{
	ring_buff_obj_t* obj = GET_RING_BUFF_OBJ(handle);

	if(obj == NULL)
	{
		return RING_BUFF_ERR_GENERAL;
	}
	ENTER_RING_BUFF_CONTEX(obj);
	/* Free will just update read pointer. It is up to the user to call it in proper order. */
	obj->read = (uint8_t*)buff + size;
	ring_buff_binary_sem_give(obj->write_sem);
	LEAVE_RING_BUFF_CONTEX(obj);

	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_read(ring_buff_handle_t handle, void** buff, uint32_t size, uint32_t *read)
{
	ring_buff_obj_t* obj = GET_RING_BUFF_OBJ(handle);
	ring_buff_err_t err = RING_BUFF_ERR_OK;

	if(handle == NULL || buff == NULL || size > obj->size || read == NULL)
	{
		return RING_BUFF_ERR_GENERAL;
	}
	ENTER_RING_BUFF_CONTEX(obj);
	/* make sure that we have enough data available */
	while(size > obj->acc_size && obj->state != RING_BUFF_STATE_STOPPED)
	{
#ifdef RING_BUFF_DBG_MSG
		printf("READ: Waiting read buffer for %u ACC: %d\n", size, obj->acc_size);
#endif
		LEAVE_RING_BUFF_CONTEX(obj);
		ring_buff_binary_sem_take(obj->read_sem);
		ENTER_RING_BUFF_CONTEX(obj);
		/* We can read, even if buffer has been stopped */
		if(ring_buff_check_state(obj, RING_BUFF_STATE_ACTIVE | RING_BUFF_STATE_STOPPED))
		{
			LEAVE_RING_BUFF_CONTEX(obj);
			return RING_BUFF_ERR_PERM;
		}
	}
	*buff = obj->acc;
	/* If writer wrapped, and we don't have enough data at the end, give as much as we can */
	if(obj->eod != NULL && (obj->acc + size > obj->eod))
	{
		*read = obj->eod - obj->acc;
		/* just a wrap (no data) available -> wrap right away and give requested size */
		if(*read == 0)
		{
			*read = size;
			*buff = obj->buff;
			obj->acc = obj->buff + *read;
		}
		else
		{
			obj->acc = obj->buff;
		}
		obj->acc_size -= *read;
		/* reset EOD */
		obj->eod = NULL;
#ifdef RING_BUFF_DBG_MSG
		printf("READ: Wrap around %u (%u) %p %p\n", *read, obj->acc_size, obj->read, obj->acc);
#endif
	}
	else
	{
		if(obj->state == RING_BUFF_STATE_STOPPED && size > obj->acc_size)
		{
#ifdef RING_BUFF_DBG_MSG
			printf("READ: Handle stopped state %u (%u)\n", size, obj->acc_size);
#endif
			size = obj->acc_size;
			err = RING_BUFF_ERR_PERM;
		}
		obj->acc_size -= size;
		obj->acc += size;
		*read = size;
	}
	LEAVE_RING_BUFF_CONTEX(obj);

	return err;
}

ring_buff_err_t ring_buff_flush(ring_buff_handle_t handle)
{
	ring_buff_obj_t* obj = GET_RING_BUFF_OBJ(handle);

	/* cannot be used if accumulation is not set */
	if(!obj->accumulate && !obj->notify_func)
	{
		return RING_BUFF_ERR_GENERAL;
	}
	/* on commit, wrap around is handled, so just send what is left */
	if(obj->acc_size != 0 && obj->accumulate && obj->notify_func)
	{
		return obj->notify_func(obj, obj->acc, obj->acc_size);
	}

	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_cancel(ring_buff_handle_t handle)
{
	ring_buff_obj_t* obj = GET_RING_BUFF_OBJ(handle);

	if(obj == NULL)
	{
		return RING_BUFF_ERR_GENERAL;
	}
	ENTER_RING_BUFF_CONTEX(obj);
	obj->state = RING_BUFF_STATE_CANCELED;
	ring_buff_binary_sem_give(obj->read_sem);
	ring_buff_binary_sem_give(obj->write_sem);
	LEAVE_RING_BUFF_CONTEX(obj);
	return RING_BUFF_ERR_OK;
}

ring_buff_err_t ring_buff_stop(ring_buff_handle_t handle)
{
	ring_buff_obj_t* obj = GET_RING_BUFF_OBJ(handle);

	if(obj == NULL)
	{
		return RING_BUFF_ERR_GENERAL;
	}
	ENTER_RING_BUFF_CONTEX(obj);
	obj->state = RING_BUFF_STATE_STOPPED;
	LEAVE_RING_BUFF_CONTEX(obj);
	return RING_BUFF_ERR_OK;
}

void ring_buff_print_err(ring_buff_err_t err)
{
	switch(err)
	{
	case RING_BUFF_ERR_GENERAL:
		fprintf(stderr, "General error happened.\n");
		break;
	case RING_BUFF_ERR_NO_MEM:
		fprintf(stderr, "Out of memory error.\n");
		break;
	case RING_BUFF_ERR_OVERRUN:
		fprintf(stderr, "Internal error - buffer overrun.\n");
		break;
	case RING_BUFF_ERR_SIZE:
		fprintf(stderr, "Data size requested wrong.\n");
		break;
	case RING_BUFF_ERR_INTERNAL:
		fprintf(stderr, "Internal general error.\n");
		break;
	case RING_BUFF_ERR_PERM:
		fprintf(stderr, "Operation not permited.\n");
		break;
	default:
		fprintf(stderr, "Unknown error code: %d.\n", err);
		break;
	}
}
