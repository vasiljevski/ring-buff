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
#include <pthread.h>

#include "message_queue.h"

typedef struct msg_box
{
	void* data;
	size_t size;
	struct msg_box *previous;
	struct msg_box *next;
} msg_box_t;

typedef struct msg_queue_obj
{
	msg_box_t* first;
	msg_box_t* last;
	unsigned int count;
	pthread_cond_t cv;
	pthread_mutex_t lock;
} msg_queue_obj_t;

#define GET_MSG_QUEUE_OBJ(handle) ((msg_queue_obj_t*)handle)

static msg_box_t* msg_box_create(void* msg_data, size_t msg_size);
static void msg_box_destroy(msg_box_t *box);

msg_queue_err_t msg_queue_create(msg_queue_hndl *handle)
{
	msg_queue_obj_t *head = (msg_queue_obj_t*)malloc(sizeof(msg_queue_obj_t));
	if(head == NULL)
	{
		*handle = NULL;
		return MSG_QUEUE_ERR_NO_MEM;
	}
	head->first = NULL;
	head->last = NULL;
	head->count = 0;
	/* Init mutex */
	if(pthread_mutex_init(&(head->lock), NULL))
	{
		free(head);
		return MSG_QUEUE_ERR_GENERAL;
	}
	/* Init cond. variable */
	if(pthread_cond_init(&(head->cv), NULL))
	{
		pthread_mutex_destroy(&(head->lock));
		free(head);
		return MSG_QUEUE_ERR_GENERAL;
	}

	*handle = head;
	return MSG_QUEUE_ERR_OK;
}

msg_queue_err_t msg_queue_destroy(msg_queue_hndl handle)
{
	msg_queue_obj_t *obj = GET_MSG_QUEUE_OBJ(handle);
	int i;
	msg_box_t *box, *next;

	pthread_mutex_lock(&(obj->lock));
	for(i=0, box=obj->first; i<obj->count; i++)
	{
		next = box->next;
		msg_box_destroy(box);
		box = next;
	}
	pthread_mutex_unlock(&(obj->lock));
	pthread_mutex_destroy(&(obj->lock));
	pthread_cond_destroy(&(obj->cv));
	free(obj);

	return MSG_QUEUE_ERR_OK;
}

msg_queue_err_t msg_queue_put(msg_queue_hndl handle, void* msg_data, size_t msg_size)
{
	msg_queue_obj_t *obj = GET_MSG_QUEUE_OBJ(handle);
	msg_box_t *box = msg_box_create(msg_data, msg_size);

	if(box == NULL)
	{
		return MSG_QUEUE_ERR_GENERAL;
	}
	pthread_mutex_lock(&(obj->lock));
	box->previous = obj->last;
	if(obj->last)
	{
		obj->last->next = box;
	}
	obj->last = box;
	if(obj->count == 0)
	{
		obj->first = box;
	}

	obj->count++;
	/* send signal that new message arrived, if someone is waiting for that */
	pthread_cond_signal(&(obj->cv));
	pthread_mutex_unlock(&(obj->lock));

	return MSG_QUEUE_ERR_OK;
}

msg_queue_err_t msg_queue_put_urgent(msg_queue_hndl handle, void* msg_data, size_t msg_size)
{
	msg_queue_obj_t *obj = GET_MSG_QUEUE_OBJ(handle);
	msg_box_t *box = msg_box_create(msg_data, msg_size);

	if(box == NULL)
	{
		return MSG_QUEUE_ERR_GENERAL;
	}
	pthread_mutex_lock(&(obj->lock));
	box->next = obj->first;
	obj->first = box;
	obj->count++;
	/* send signal that new message arrived, if someone is waiting for that */
	pthread_cond_signal(&(obj->cv));
	pthread_mutex_unlock(&(obj->lock));

	return MSG_QUEUE_ERR_OK;
}

msg_queue_err_t msg_queue_get(msg_queue_hndl handle, void** msg_data, size_t* msg_size)
{
	msg_queue_obj_t *obj = GET_MSG_QUEUE_OBJ(handle);
	msg_box_t *box;

	pthread_mutex_lock(&(obj->lock));
	if(obj->count == 0)
	{
		pthread_cond_wait(&(obj->cv), &(obj->lock));
	}
	box = obj->first;
	obj->first = box->next;
	*msg_data = box->data;
	*msg_size = box->size;
	msg_box_destroy(box);
	obj->count--;
	if(obj->count == 0)
	{
		obj->last = NULL;
	}
	pthread_mutex_unlock(&(obj->lock));
	return MSG_QUEUE_ERR_OK;
}

static msg_box_t* msg_box_create(void* msg_data, size_t msg_size)
{
	msg_box_t *box = (msg_box_t*) malloc(sizeof(msg_box_t));
	if(box == NULL)
	{
		return NULL;
	}
	box->data = msg_data;
	box->size = msg_size;
	box->next = NULL;
	box->previous = NULL;

	return box;
}

static void msg_box_destroy(msg_box_t *box)
{
	free(box);
}
