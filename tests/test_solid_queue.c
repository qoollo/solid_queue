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
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <check.h>
#include <unistd.h>
#include <ftw.h>
#include <include/solid_queue.h>

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
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

START_TEST(test_push_to_queue)
{
	queue_param_t queue_param;
	queue_param.eblob_param.blob_size_limit = 2000000000;
	queue_param.eblob_param.blob_size = 200000000;
	queue_param.eblob_param.records_in_blob = queue_param.eblob_param.blob_size/14000;
	queue_param.eblob_param.sync = 5;
	queue_param.eblob_param.defrag_timeout = 12;
	queue_param.eblob_param.defrag_percentage = 25;
	queue_param.eblob_param.blob_flags = EBLOB_TIMED_DATASORT;
	queue_param.num_of_records = 20000;

	if(mkdir("/tmp/queue_for_tests", 0700) != 0)
	{
		printf("Mkdir: error\n");
	}
	queue_param.eblob_param.file = "/tmp/queue_for_tests";
	queue_param.eblob_param.log_level = EBLOB_LOG_INFO;
	solid_queue_t *solid_queue = queue_open(queue_param);

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
	rmrf("/tmp/queue_for_tests");
}
END_TEST

START_TEST(test_pushes_to_queue)
{
	queue_param_t queue_param;
	queue_param.eblob_param.blob_size_limit = 2000000000;
	queue_param.eblob_param.blob_size = 200000000;
	queue_param.eblob_param.records_in_blob = queue_param.eblob_param.blob_size/14000;
	queue_param.eblob_param.sync = 5;
	queue_param.eblob_param.defrag_timeout = 12;
	queue_param.eblob_param.defrag_percentage = 25;
	queue_param.eblob_param.blob_flags = EBLOB_TIMED_DATASORT;
	queue_param.num_of_records = 20000;
	if(mkdir("/tmp/queue_for_tests", 0700) != 0)
	{
		printf("Mkdir: error\n");
	}
	queue_param.eblob_param.file = "/tmp/queue_for_tests";
	queue_param.eblob_param.log_level = EBLOB_LOG_INFO;
	solid_queue_t *solid_queue = queue_open(queue_param);

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
	rmrf("/tmp/queue_for_tests");
}
END_TEST

Suite * queue_suite(void)
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("DiskQueue");

	/* Core test case */
	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_push_to_queue);
	tcase_add_test(tc_core, test_pushes_to_queue);
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
