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
#define _XOPEN_SOURCE 700
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif


#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include <unistd.h>
#include <ftw.h>
#include <pthread.h>
#include <errno.h>
#include "solid_queue.h"
 
struct parameters_t
{
	int write_quantity;
	int read_quantity;
	int counter;
	solid_queue_t *queue;
};

pthread_t tid[3];

void* writing(void *p)
{
	struct parameters_t *parameters = (struct parameters_t*)p;
	if(!parameters || !parameters->queue)
	{
		return NULL;
	}
	bool was_overwrite = false;
	for(int i = 0; i < parameters->write_quantity; ++i)
	{
		char ch = 'a';
		queue_push(parameters->queue, &ch, sizeof(char), &was_overwrite);
	}
	return NULL;
}

void* reading(void *p)
{
	struct parameters_t *parameters = (struct parameters_t*)p;
	if(!parameters || !parameters->queue)
	{
		return NULL;
	}
	parameters->counter = 0;
	for(int i = 0; i < parameters->read_quantity; ++i)
	{
		size_t len;
		void *data;
		if(queue_pull(parameters->queue, &data, &len) == 0)
		{
			if(*(char*)data == 'a')
			++parameters->counter;
			free(data);
		}
	}
	return NULL;
}

int unlink_cb(const char *fpath, const struct stat* UNUSED(sb), int UNUSED(typeflag), struct FTW* UNUSED(ftwbuf))
{
	int rv = remove(fpath);

	if (rv)
		perror(fpath);

	return rv;
}

int rmrf(char *path)
{
	return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

solid_queue_t *init_test_queue(int queue_max_length)
{
	char *temp_name = (char *) malloc(22);
	strncpy(temp_name, "/tmp/myTmpFile-XXXXXX", 21);
	queue_param_t queue_param;
	memset(&queue_param, 0, sizeof(queue_param));

	queue_param.eblob_param.blob_size_limit = 200000000;
	queue_param.eblob_param.blob_size = 20000000;
	queue_param.eblob_param.records_in_blob = queue_param.eblob_param.blob_size/20000;
	queue_param.eblob_param.sync = 5;
	queue_param.eblob_param.defrag_timeout = 12;
	queue_param.eblob_param.defrag_percentage = 25;
	queue_param.eblob_param.blob_flags = EBLOB_TIMED_DATASORT;
	queue_param.max_queue_length = (uint64_t)queue_max_length;
	queue_param.time_to_wait = 10;
	if(!mkdtemp(temp_name))
	{
		printf("Mkdtemp: error %i\n", errno);
	}
	queue_param.eblob_param.path = temp_name;
	queue_param.eblob_param.log_level = EBLOB_LOG_ERROR;
	return queue_open(queue_param);
}

START_TEST(test_push_to_queue)
{
	solid_queue_t *solid_queue = init_test_queue(100);
	void *data;
	uint64_t len;
	int err = 0;
	bool was_overwrite = 0;
	err = queue_push(solid_queue, "a", 2, &was_overwrite);
	ck_assert_msg(err == 0, "push failed. Error returned %i.", err);
	queue_pull(solid_queue, &data, &len);
	ck_assert_msg(strcmp(data, "a") == 0, "failure. Wrong returned data: %s.", data);
	ck_assert_msg(len == 2, "failure. Wrong returned length of data.");
	queue_close(solid_queue);
}
END_TEST

START_TEST(test_queue_length)
{
	solid_queue_t *q = init_test_queue(4);
	void *data;
	uint64_t len;
	int err = 0;
	bool was_overwrite = 0;
	int i = 0;
	for(i = 0; i < 3; i++)
	{
		err = queue_push(q, "a", 2, &was_overwrite);
		ck_assert_msg(err == 0, "push failed. Error returned %i.", err);
	}
	ck_assert_msg(queue_length(q) == 3, "queue lenght mismatch, after pushed 3 items got length %i.", queue_length(q));
	for(i = 0; i < 2; i++)
	{
		err = queue_pull(q, &data, &len);
		ck_assert_msg(err == 0, "pull failed. Error returned %i.", err);
	}
	ck_assert_msg(queue_length(q) == 1, "queue lenght mismatch, after pulled 2 items got length %i", queue_length(q));
	for(i = 0; i < 6; i++)
	{
		err = queue_push(q, "a", 2, &was_overwrite);
		ck_assert_msg(err == 0, "push failed. Error returned %i.", err);
	}
	ck_assert_msg(queue_length(q) == 4, "queue lenght mismatch, after pushed more then size got length %i", queue_length(q));
	queue_close(q);

}
END_TEST

START_TEST(test_pushes_to_queue)
{
	solid_queue_t *solid_queue = init_test_queue(5);
	void *data;
	uint64_t len;
	int err = 0;
	bool was_overwrite = 0;
	err = queue_push(solid_queue, "a", 2, &was_overwrite);
	ck_assert_msg(err == 0, "push failed. Error returned %i", err);
	err = queue_push(solid_queue, "bb", 3, &was_overwrite);
	ck_assert_msg(err == 0, "push failed. Error returned %i", err);
	err = queue_push(solid_queue, "ccc", 4, &was_overwrite);
	ck_assert_msg(err == 0, "push failed. Error returned %i", err);

	queue_pull(solid_queue, &data, &len);
	ck_assert_msg(strcmp(data, "a") == 0, "failure. Wrong returned data.");
	ck_assert_msg(len == 2, "failure. Wrong returned length of data.");
	free(data);
	data = NULL;

	queue_pull(solid_queue, &data, &len);
	ck_assert_msg(strcmp(data, "bb") == 0, "failure. Wrong returned data.");
	ck_assert_msg(len == 3, "failure. Wrong returned length of data.");
	free(data);
	data = NULL;

	queue_pull(solid_queue, &data, &len);
	ck_assert_msg(strcmp(data, "ccc") == 0, "failure. Wrong returned data.");
	ck_assert_msg(len == 4, "failure. Wrong returned length of data.");
	free(data);
	data = NULL;
}
END_TEST

START_TEST(test_of_thread_safety)
	{
		struct parameters_t *param = (struct parameters_t*) malloc(sizeof(struct parameters_t));
		memset(param, 0, sizeof(struct parameters_t));
		param->queue = init_test_queue(700);
		param->write_quantity = 256;
		param->read_quantity = 512;
		if(pthread_create(&tid[0], NULL, writing, param) != 0 ||
		   pthread_create(&tid[1], NULL, writing, param) != 0 ||
		   pthread_create(&tid[2], NULL, reading, param) != 0)
		{
			ck_abort_msg("failure. Pthread creating.");
		}
		pthread_join(tid[0], NULL);
		pthread_join(tid[1], NULL);
		pthread_join(tid[2], NULL);
		ck_assert_msg(param->counter == param->read_quantity, "failure. Bad read count: %i\n", param->counter);
		queue_close(param->queue);
		free(param);
	}
END_TEST

Suite * queue_suite(void)
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("Solid_queue");

	/* Core test case */
	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_push_to_queue);
	tcase_add_test(tc_core, test_pushes_to_queue);
	tcase_add_test(tc_core, test_queue_length);
	tcase_add_test(tc_core, test_of_thread_safety);
	suite_add_tcase(s, tc_core);

	return s;
}

int main()
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = queue_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
