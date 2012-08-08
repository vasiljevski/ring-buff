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
#include <time.h>
#include <pthread.h>

#define __USE_BSD
#include <unistd.h>

#include "ring_buff.h"
#include "message_queue.h"

#define FIRST_TC_BUFF_SIZE (50*1024)
#define FIRST_TC_LOOPS     (3000)

#define SECOND_TC_BUFF_SIZE (64*1024)
#define SECOND_TC_ACC_SIZE  (24*1024)
#define SECOND_TC_LOOPS     (5000)

#define DEMUX_CRC_ADDER_MASK    0x04C11DB7  /* As defined in MPEG-2 CRC     */
                                            /* Decoder Model:               */
                                            /*   1 - adder is enabled       */
                                            /*   0 - adder is disabled      */

static unsigned int uCRCTable[256];

static void Init_CRC(void)
{
	unsigned int crc;
	int i, j;

	/* Initialize CRC table */
	for(i=0; i < 256; i++)
	{
		crc= 0;
		for(j=7; j >= 0; j--)
		{
			if(((i >> j) ^ (crc >> 31)) & 1)
			{
				crc = (crc << 1) ^ DEMUX_CRC_ADDER_MASK;
			}
			else
			{
				crc <<= 1;
			}
		}
		uCRCTable[i] = crc;
	}
}

static unsigned int CalculateCRC(const unsigned char *data, unsigned int len)
{
	unsigned int crc = 0xFFFFFFFF;
	unsigned int i;

	for (i = 0; i < len; ++i)
	{
		crc = (crc << 8) ^ uCRCTable[(crc >> 24) ^ (*data++)];
	}

	return (crc);
}

/* ####### First test case: This is general "provider/consumer" test case. ####### */

typedef struct first_tc_msg
{
	unsigned int size;
	unsigned int crc;
} first_tc_msg_t;

typedef struct _tc_arg
{
	ring_buff_handle_t ring_buff;
	unsigned int loops;
	unsigned int failed;
} tc_arg_t;


void* first_tc_provider(void* arg)
{
	ring_buff_handle_t ring_buff = ((tc_arg_t*) arg)->ring_buff;
	int tc_run = ((tc_arg_t*) arg)->loops;
	unsigned int size = 0;
	first_tc_msg_t* msg;
	unsigned char* data;
	int i;
	ring_buff_err_t err;

	while(tc_run)
	{
		size = rand() % 16384 + 1024;
		err = ring_buff_reserve(ring_buff, (void**)&msg, (unsigned int)sizeof(first_tc_msg_t));
		if(err != RING_BUFF_ERR_OK)
		{
			printf("************* ERROR reserving message **************\n");
			ring_buff_print_err(err);
			return NULL;
		}
		msg->size = size;
		err = ring_buff_reserve(ring_buff, (void**)&data, size);
		if(err != RING_BUFF_ERR_OK)
		{
			printf("*************** ERROR reserving data ***************\n");
			ring_buff_print_err(err);
			return NULL;
		}
		for(i=0; i<size; i++)
		{
			data[i] = (unsigned char) (rand() % 0xFF);
		}
		msg->crc = CalculateCRC(data, size);
		err = ring_buff_commit(ring_buff, msg, sizeof(first_tc_msg_t));
		if(err != RING_BUFF_ERR_OK)
		{
			printf("************* ERROR committing message *************\n");
			ring_buff_print_err(err);
			return NULL;
		}
		err = ring_buff_commit(ring_buff, data, size);
		if(err != RING_BUFF_ERR_OK)
		{
			printf("************** ERROR committing data ***************\n");
			ring_buff_print_err(err);
			return NULL;
		}
		/* usleep(rand() % 100000 + 100); */
		tc_run--;
	}
	err = ring_buff_reserve(ring_buff, (void**)&msg, (unsigned int)sizeof(first_tc_msg_t));
	if(err != RING_BUFF_ERR_OK)
	{
		printf("************* ERROR reserving message **************\n");
		ring_buff_print_err(err);
		return NULL;
	}
	/* send "End Of Test" message */
	msg->size = 0;
	msg->crc = 0;
	err = ring_buff_commit(ring_buff, msg, sizeof(first_tc_msg_t));
	if(err != RING_BUFF_ERR_OK)
	{
		printf("************* ERROR committing message *************\n");
		ring_buff_print_err(err);
		return NULL;
	}

	return NULL;
}

void* first_tc_consumer(void* arg)
{
	tc_arg_t* tc_arg = (tc_arg_t*) arg;
	ring_buff_handle_t ring_buff = tc_arg->ring_buff;
	first_tc_msg_t* msg;
	unsigned char* data;
	unsigned int crc = 0;
	unsigned int count = 1;
	unsigned int read;
	ring_buff_err_t err;

	while(1)
	{
		err = ring_buff_read(ring_buff, (void**)&msg, sizeof(first_tc_msg_t), &read);
		if(err != RING_BUFF_ERR_OK)
		{
			printf("************** ERROR reading message ***************\n");
			ring_buff_print_err(err);
			return NULL;
		}
		if(msg->size == 0 && msg->crc == 0)
		{
			printf("************** End Of Test received ****************\n");
			break;
		}
		err = ring_buff_read(ring_buff, (void**)&data, msg->size, &read);
		if(err != RING_BUFF_ERR_OK)
		{
			printf("*************** ERROR reading data *****************\n");
			ring_buff_print_err(err);
			return NULL;
		}
		crc = CalculateCRC(data, msg->size);
		if(crc != msg->crc)
		{
			printf("** %04u: FAILED (CRC exp/rd: 0x%08x/0x%08x) **\n", count, msg->crc, crc);
			tc_arg->failed++;
		}
		else
		{
			printf("********* %04u: PASSED (CRC: 0x%08x) ***********\n", count, crc);
		}
		err = ring_buff_free(ring_buff, msg, sizeof(first_tc_msg_t));
		if(err != RING_BUFF_ERR_OK)
		{
			printf("************** ERROR freeing message ***************\n");
			ring_buff_print_err(err);
			return NULL;
		}
		err = ring_buff_free(ring_buff, (void*)data, msg->size);
		if(err != RING_BUFF_ERR_OK)
		{
			printf("*************** ERROR freeing data *****************\n");
			ring_buff_print_err(err);
			return NULL;
		}
		count++;
		usleep(rand() % 100000 + 100);
	}
	tc_arg->loops = count - 1;

	return NULL;
}

static void execute_first_tc(void)
{
	pthread_t provider;
	pthread_t consumer;
	pthread_attr_t attr;
	ring_buff_handle_t ring_buff = NULL;
	ring_buff_attr_t   ring_buff_attr = {NULL, FIRST_TC_BUFF_SIZE, 0, NULL};
	tc_arg_t tc_arg = {NULL, FIRST_TC_LOOPS, 0};
	ring_buff_err_t err;
	void *buff;

	if((buff = malloc(FIRST_TC_BUFF_SIZE)) == NULL)
	{
		printf("****************** ERROR no memory *****************\n");
		goto done;
	}
	ring_buff_attr.buff = buff;
	printf("********** Executing blocking read/write test **********\n");
	err = ring_buff_create(&ring_buff_attr, &ring_buff);
	if(err != RING_BUFF_ERR_OK)
	{
		printf("************** ERROR creating ring buffer **************\n");
		ring_buff_print_err(err);
		goto done;
	}
	srand ( time(NULL) );
	Init_CRC();
	tc_arg.ring_buff = ring_buff;
	pthread_attr_init(&attr);
	if (pthread_create(&provider, &attr, first_tc_provider, &tc_arg) != 0)
	{
		printf("************ ERROR creating provider thread *************\n");
		pthread_attr_destroy(&attr);
		ring_buff_destroy(ring_buff);
		goto done;
	}
	if (pthread_create(&consumer, &attr, first_tc_consumer, &tc_arg) != 0)
	{
		printf("************ ERROR creating consumer thread *************\n");
		pthread_attr_destroy(&attr);
		ring_buff_destroy(ring_buff);
		goto done;
	}
	pthread_join(provider, NULL);
	pthread_join(consumer, NULL);
	ring_buff_destroy(ring_buff);
	pthread_attr_destroy(&attr);

done:
	if(buff != NULL)
	{
		free(buff);
	}
	printf(" LOOPS:  %u\n", tc_arg.loops);
	printf(" FAILED: %u\n", tc_arg.failed);
	printf("************************* DONE *************************\n");
}

/* lets save message if */
static first_tc_msg_t second_tc_save_msg = {0, 0};
static unsigned int second_tc_count = 0;
static unsigned int second_tc_failed = 0;

void* second_tc_provider(void* arg)
{
	ring_buff_handle_t ring_buff = ((tc_arg_t*) arg)->ring_buff;
	ring_buff_err_t err;

	/* use same provider as for regular read/write test */
	first_tc_provider(arg);
	/* we have to be sure that all data is sent */
	err = ring_buff_flush(ring_buff);
	if(err != RING_BUFF_ERR_OK)
	{
		ring_buff_print_err(err);
	}

	return NULL;
}

ring_buff_err_t second_tc_notify(ring_buff_handle_t handle, void* buff, unsigned int size)
{
	static first_tc_msg_t* msg;
	unsigned int crc = 0, tmp_size;
	unsigned char save_msg = 0;
	ring_buff_err_t err;

	/* check for leftovers from previous notify */
	if(second_tc_save_msg.crc != 0 && second_tc_save_msg.size != 0)
	{
		crc = CalculateCRC(buff, second_tc_save_msg.size);
		second_tc_count++;
		if(crc != second_tc_save_msg.crc)
		{
			printf("** %04u: FAILED (CRC exp/rd: 0x%08x/0x%08x) **\n", second_tc_count, second_tc_save_msg.crc, crc);
			second_tc_failed++;
		}
		else
		{
			printf("********* %04u: PASSED (CRC: 0x%08x) ***********\n", second_tc_count, crc);
		}
		err = ring_buff_free(handle, buff, second_tc_save_msg.size);
		if(err != RING_BUFF_ERR_OK)
		{
			printf("**************** ERROR freeing data ****************\n");
			ring_buff_print_err(err);
			return RING_BUFF_ERR_GENERAL;
		}
		size -= second_tc_save_msg.size;
		buff = (uint8_t*)buff + second_tc_save_msg.size;
		second_tc_save_msg.size = 0;
		second_tc_save_msg.crc = 0;
	}
	while(size > 0)
	{
		msg = (first_tc_msg_t*) buff;
		buff = (uint8_t*)buff + sizeof(first_tc_msg_t);
		size -= sizeof(first_tc_msg_t);
		if(msg->crc == 0 && msg->size == 0)
		{
			printf("************** End Of Test received ****************\n");
			/* force exit */
			size = 0;
		}
		if(size == 0)
		{
			save_msg = 1;
			break;
		}
		crc = CalculateCRC(buff, msg->size);
		if(crc != msg->crc)
		{
			printf("** %04u: FAILED (CRC exp/rd: 0x%08x/0x%08x) **\n", second_tc_count, msg->crc, crc);
			second_tc_failed++;
		}
		else
		{
			printf("********* %04u: PASSED (CRC: 0x%08x) ***********\n", second_tc_count, crc);
		}
		tmp_size = msg->size;
		err = ring_buff_free(handle, msg, sizeof(first_tc_msg_t));
		if(err != RING_BUFF_ERR_OK)
		{
			printf("************** ERROR freeing message ***************\n");
			ring_buff_print_err(err);
			return RING_BUFF_ERR_GENERAL;
		}
		err = ring_buff_free(handle, buff, tmp_size);
		if(err != RING_BUFF_ERR_OK)
		{
			printf("**************** ERROR freeing data ****************\n");
			ring_buff_print_err(err);
			return RING_BUFF_ERR_GENERAL;
		}
		size -= msg->size;
		buff = (uint8_t*)buff + msg->size;
		second_tc_count++;
	}
	if(save_msg)
	{
		second_tc_save_msg.crc = msg->crc;
		second_tc_save_msg.size = msg->size;
		err = ring_buff_free(handle, msg, sizeof(first_tc_msg_t));
		if(err != RING_BUFF_ERR_OK)
		{
			printf("************** ERROR freeing message ***************\n");
			ring_buff_print_err(err);
			return RING_BUFF_ERR_GENERAL;
		}
	}
	if(size < 0)
	{
		printf("************ Notify: protocol ERROR!!! *************\n");
	}

	return RING_BUFF_ERR_OK;
}


static void execute_second_tc(void)
{
	pthread_t provider;
	pthread_attr_t attr;
	ring_buff_handle_t ring_buff;
	ring_buff_attr_t   ring_buff_attr = {NULL, SECOND_TC_BUFF_SIZE, SECOND_TC_ACC_SIZE, second_tc_notify};
	tc_arg_t tc_arg = {NULL, SECOND_TC_LOOPS, 0};
	ring_buff_err_t err;
	void *buff;

	if((buff = malloc(SECOND_TC_BUFF_SIZE)) == NULL)
	{
		printf("****************** ERROR no memory *****************\n");
		goto done;
	}
	ring_buff_attr.buff = buff;
	printf("************* Executing reader notify test *************\n");
	err = ring_buff_create(&ring_buff_attr, &ring_buff);
	if(err != RING_BUFF_ERR_OK)
	{
		printf("************** ERROR creating ring buffer **************\n");
		ring_buff_print_err(err);
		goto done;
	}
	srand(time(NULL));
	Init_CRC();
	tc_arg.ring_buff = ring_buff;
	pthread_attr_init(&attr);
	/* reuse same provider as for regular read/write */
	if (pthread_create(&provider, &attr, second_tc_provider, &tc_arg) != 0)
	{
		printf("************ ERROR creating provider thread *************\n");
		pthread_attr_destroy(&attr);
		ring_buff_destroy(ring_buff);
		goto done;
	}
	pthread_join(provider, NULL);
	ring_buff_destroy(ring_buff);
	pthread_attr_destroy(&attr);

done:
	if(buff != NULL)
	{
		free(buff);
	}
	printf(" LOOPS:  %u\n", second_tc_count);
	printf(" FAILED: %u\n", second_tc_failed);
	printf("************************* DONE *************************\n");
}


#define FOURTH_TC_LOOPS     (3000)

typedef struct fourth_tc_msg
{
	unsigned int size;
	unsigned int crc;
} fourth_tc_msg_t;

typedef struct _fourth_tc_arg
{
	msg_queue_hndl queue;
	unsigned int loops;
	unsigned int failed;
} fourth_tc_arg_t;

void* fourth_tc_provider(void* arg)
{
	fourth_tc_arg_t *tc_arg = (fourth_tc_arg_t *) arg;
	int i, *to_put;

	for(i=0; i<FOURTH_TC_LOOPS; i++)
	{
		to_put = (int*)malloc(sizeof(int));
		*to_put = i;
		if(msg_queue_put(tc_arg->queue, to_put, sizeof(int)) != MSG_QUEUE_ERR_OK)
		{
			printf("**************** ERROR sending message *****************\n");
			return NULL;
		}
		usleep(rand() % 10000 + 100);
	}
	return NULL;
}

void* fourth_tc_consumer(void* arg)
{
	fourth_tc_arg_t *tc_arg = (fourth_tc_arg_t *) arg;
	int *msg, i;
	size_t size;

	for(i=0; i<FOURTH_TC_LOOPS; i++)
	{
		if(msg_queue_get(tc_arg->queue, (void*)&msg, &size) != MSG_QUEUE_ERR_OK)
		{
			printf("**************** ERROR sending message *****************\n");
			return NULL;
		}
		if(i != *msg)
		{
			tc_arg->failed++;
		}
		free(msg);
		tc_arg->loops++;
		usleep(rand() % 10000 + 100);
	}
	return NULL;
}

static void execute_fourth_tc(void)
{
	pthread_t provider;
	pthread_t consumer;
	pthread_attr_t attr;
	fourth_tc_arg_t tc_arg = {NULL, 0, 0};

	if(msg_queue_create(&tc_arg.queue) != MSG_QUEUE_ERR_OK)
	{
		printf("************* ERROR creating message queue **************\n");
		goto done;
	}
	tc_arg.loops = 0;
	tc_arg.failed = 0;
	pthread_attr_init(&attr);
	if (pthread_create(&provider, &attr, fourth_tc_provider, &tc_arg) != 0)
	{
		printf("************ ERROR creating provider thread *************\n");
		pthread_attr_destroy(&attr);
		goto done;
	}
	if (pthread_create(&consumer, &attr, fourth_tc_consumer, &tc_arg) != 0)
	{
		printf("************ ERROR creating consumer thread *************\n");
		pthread_attr_destroy(&attr);
		goto done;
	}
	pthread_attr_destroy(&attr);


	pthread_join(provider, NULL);
	pthread_join(consumer, NULL);
done:
	if(tc_arg.queue)
	{
		msg_queue_destroy(tc_arg.queue);
	}
	printf(" LOOPS:  %u\n", tc_arg.loops);
	printf(" FAILED: %u\n", tc_arg.failed);
	printf("************************* DONE *************************\n");
}

static void print_help(void)
{
	printf("********** Ring buffer test **************\n");
	printf("Please provide test case number:\n");
	printf("1) Regular (blocking) read/write test\n");
	printf("2) Notify reader on N bytes written test\n");
	printf("3) Stream from HTTP server with CURL\n");
	printf("4) Message queue test\n");
	printf("******************************************\n");
}

int main(int argc, char** argv)
{
	int tc = 0;

	if(argc != 2)
	{
		print_help();
		return -1;
	}
	tc = atoi(argv[1]);
	switch(tc)
	{
	case 1:
		execute_first_tc();
		break;
	case 2:
		execute_second_tc();
		break;
	case 3:
		printf("To be done...\n");
		break;
	case 4:
		execute_fourth_tc();
		break;
	default:
		print_help();
		return -1;
	}
	return 0;
}
