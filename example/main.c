/*
 * Copyright 2014+ Dmitry Rudnev <rudneff.d@gmail.com>
 *
 * This file is part of Solid_queue.
 *
 * Solid_queue is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Solid_queue is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Leser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Solid_queue.  If not, see <http://www.gnu.org/licenses/>.
 */
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "solid_queue.h"

struct perf_stat
{
	volatile uint32_t written_count;
	volatile uint32_t read_count;
	volatile uint32_t write_error_count;
	volatile uint32_t read_error_count;
} perf_stat_instance;

struct parameters_t
{
	int quantity;
	int data_size;
	int data_variance;
	solid_queue_t *queue;
};

volatile uint32_t cnt = 0;
unsigned char is_exiting = 0;
pthread_t tid[5];

void* perf_stat_thread(void *t)
{
	int total = *((int*)t);
	unsigned int last_written = 0;
	unsigned int last_read = 0;

	while(!is_exiting)
	{
		unsigned int write_speed = perf_stat_instance.written_count - last_written;
		unsigned int read_speed = perf_stat_instance.read_count - last_read;

		double percentage = ((double)(perf_stat_instance.written_count +
									  perf_stat_instance.write_error_count)/
									  (double)total) * 100;
		printf("Stat: W: done: %f ", percentage);
		printf("recs: %i wps: %i ", perf_stat_instance.written_count, write_speed);
		printf("err: %i ", perf_stat_instance.write_error_count);
		percentage = ((double)(perf_stat_instance.read_count +
							   perf_stat_instance.read_error_count)/
							   (double)total) * 100;
		printf("R: done: %f reads: %i ", percentage, perf_stat_instance.read_count);
		printf("rps: %i err: %i\n", read_speed, perf_stat_instance.read_error_count);
		last_written = perf_stat_instance.written_count;
		last_read = perf_stat_instance.read_count;
		sleep(1);
	}
	return NULL;
}

void* write_thread(void *p)
{
	struct parameters_t *parameters = (struct parameters_t*)p;
	if(!parameters || !parameters->queue)
	{
		return NULL;
	}
	int max_size_of_data = parameters->data_size + parameters->data_variance;
	char testval[max_size_of_data];
	for(int i = 0; i < max_size_of_data; ++i)
	{
		testval[i] = 'a';
	}
	testval[max_size_of_data-1] = '\0';
	int min_size_of_data = parameters->data_size - parameters->data_variance;
	int difference = parameters->data_variance * 2;
	srand(time(NULL));
	bool was_overwrite = 0;
	for(int i = 0; i < parameters->quantity; ++i)
	{
		if(queue_push(parameters->queue, testval, min_size_of_data + (rand() % difference), &was_overwrite) != 0 )
		{
			__sync_fetch_and_add(&(perf_stat_instance.write_error_count), 1);
			printf("Write thread %" PRIu64 ": going sleep!\n", get_thread_id());
			sleep(3);
		}
		else
		{
			__sync_fetch_and_add(&(perf_stat_instance.written_count), 1);
		}
	}
	return NULL;
}

void* read_thread(void *p)
{
	struct parameters_t *parameters = (struct parameters_t*)p;
	if(!parameters || !parameters->queue)
	{
		return NULL;
	}

	for(int i = 0; i < parameters->quantity; ++i)
	{
		size_t len;
		void *data;
		if(queue_pull(parameters->queue, &data, &len) != 0)
		{
			__sync_fetch_and_add(&(perf_stat_instance.read_error_count), 1);
			printf("Read thread %" PRIu64 " : Error. Going sleep!\n", get_thread_id());
			sleep(3);
		}
		else
		{
			free(data);
			__sync_fetch_and_add(&(perf_stat_instance.read_count), 1);
		}
	}
	return NULL;
}

int main(int argc, char **argv)
{
	if(argc != 5)
	{
		printf("Using: {path_to_queue} {num_of_records} {size_of_rec} {variance_of_rec}\n");
		exit(1);
	}
	struct parameters_t *param = (struct parameters_t*) malloc (sizeof(struct parameters_t));
	if(!param)
	{
		printf("Error while creating parameters.\n");
		exit(1);
	}

	int arg_index = 0;
	queue_param_t queue_param;
	queue_param.eblob_param.blob_size_limit = 1500000000;
	queue_param.eblob_param.blob_size = 200000000;
	queue_param.eblob_param.records_in_blob = queue_param.eblob_param.blob_size/14000;
	queue_param.eblob_param.sync = 5;
	queue_param.eblob_param.defrag_timeout = 12;
	queue_param.eblob_param.defrag_percentage = 25;
	queue_param.eblob_param.blob_flags = EBLOB_TIMED_DATASORT;
	queue_param.eblob_param.file = argv[++arg_index];
	queue_param.time_to_wait = 5;
	queue_param.num_of_records = 20000;
	param->quantity = atoi(argv[++arg_index]);
	param->data_size = atoi(argv[++arg_index]);
	param->data_variance = atoi(argv[++arg_index]);

	queue_param.eblob_param.log_level = EBLOB_LOG_ERROR;
	if(!(param->queue = queue_open(queue_param)))
	{
		exit(1);
	}
	if(pthread_create(&tid[0], NULL, perf_stat_thread, &(param->quantity)) != 0 ||
	   pthread_create(&tid[1], NULL, write_thread, param) != 0 ||
	   pthread_create(&tid[2], NULL, read_thread, param) != 0 ||
	   pthread_create(&tid[3], NULL, write_thread, param) != 0 ||
	   pthread_create(&tid[4], NULL, read_thread, param) != 0)
	{
		printf("Can't create thread.\n");
		exit(1);
	}

	pthread_join(tid[1], NULL);
	pthread_join(tid[2], NULL);
	pthread_join(tid[3], NULL);
	pthread_join(tid[4], NULL);
	is_exiting = 1;
	pthread_join(tid[0], NULL);
	queue_close(param->queue);
	free(param);
	printf("All done.\n");

	return 0;
}

