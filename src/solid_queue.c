/*
 * Copyright 2014+ Dmitry Rudnev <rudneff.d@gmail.com>
 * Copyright 2014+ Kirill Bushminkin <kbushminkin@gmail.com>
 *
 * This file is part of library solid_queue.
 *
 * Solid_queue is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Solid_queue is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Solid_queue.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file solid_queue.c
 * @author Dmitry Rudnev
 * @date 18 Oct 2014
 * @brief File contains implementation of persistent queue interface.
 */
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
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
#include <errno.h>
#include <semaphore.h>
#include <time.h>
#include "solid_queue.h"

/**
 * @brief Pointer to iterator handler typedef.
 */
typedef int (*iter_f)(struct eblob_disk_control *dc,
					  struct eblob_ram_control *ctl,
					  void *data,
					  void *priv,
					  void *thread_priv);

struct _max_min_t
{
	uint64_t max, min;
};

/**
 * @brief Queue structure.
 */
struct _solid_queue_t
{
	struct eblob_backend *eback;
	uint64_t first_key;             /**< Number of first element in queue. */
	uint64_t last_key;              /**< Number of last element in queue. */
	uint64_t length;                /**< Quantity of records in queue. */
	pthread_mutex_t use_queue;      /**< Mutex allowed use queue for only one thread. */
	uint64_t count_remaining;       /**< Count of free cells in queue. */
	sem_t lock_on_empty;            /**< Semaphore prohibits to pull from empty queue. */
	int time_to_wait;               /**< Timer is used for semaphore and mutex timedlock. */
};

uint64_t get_thread_id()
{
	pthread_t ptid = pthread_self();
	return (uint64_t) ptid;
}

/**
 * @brief Iterator handler.
 */
int iterator_h(struct eblob_disk_control *dc,
			   struct eblob_ram_control* UNUSED(ctl),
			   void* UNUSED(data),
			   void *priv,
			   void* UNUSED(thread_priv))
{
	uint64_t id;
	memcpy(&id, &(dc->key), sizeof(id));
	struct _max_min_t *t= (struct _max_min_t*)priv;
	if(t->max < id && id != 0)
	{
		t->max = id;
	}
	if(t->min > id && id != 0)
	{
		t->min = id;
	}
	return 0;
}

int eblob_log_init (struct eblob_log **el, int level, void *priv, log_handler_t log_h)
{
	if(*el) return EINVAL;
	if(!(*el = (struct eblob_log*) malloc (sizeof(struct eblob_log))))
	{
		return ENOMEM;
	}
	(*el)->log_level = level;
	(*el)->log_private = priv;
	(*el)->log = log_h;
	return 0;
}


int eblob_config_init(struct eblob_config **econf, const eblob_param_t eblob_param, struct eblob_log *log)
{
	if(*econf) return EINVAL;
	if(!(*econf = (struct eblob_config *) malloc (sizeof(struct eblob_config))))
	{
		return ENOMEM;
	}
	char *fullpath = NULL;
	if(!(fullpath = (char*) malloc (strlen(eblob_param.path) + strlen("/data"))))
	{
		free(econf);
		return ENOMEM;
	}
	strcpy(fullpath, eblob_param.path);
	strcat(fullpath, "/data");
	(*econf)->blob_flags = eblob_param.blob_flags;
	(*econf)->file = fullpath;
	(*econf)->blob_size = eblob_param.blob_size;
	(*econf)->blob_size_limit = eblob_param.blob_size_limit;
	(*econf)->records_in_blob = eblob_param.records_in_blob;
	(*econf)->log = log;
	(*econf)->sync = eblob_param.sync;
	(*econf)->defrag_percentage = eblob_param.defrag_percentage;
	(*econf)->defrag_timeout = eblob_param.defrag_timeout;
	return 0;
}

void eblob_config_free(struct eblob_config *econf)
{
	if(econf)
	{
		free(econf->file);
		free(econf);
	}
}

int iterate_queue(struct _solid_queue_t *q, iter_f iter_func, struct eblob_log *l)
{
	if(!q || !iter_func)
	{
		return -EFAULT;
	}
	q->first_key = 0;
	q->last_key = 0;
	struct eblob_iterate_control iter;
	memset(&iter, 0, sizeof(iter));
	iter.b = q->eback;
	iter.log = l;
	iter.iterator_cb.iterator = iter_func;
	struct _max_min_t max_min;
	memset(&max_min, 0, sizeof(struct _max_min_t));
	max_min.max = 0;
	max_min.min = UINT64_MAX;
	iter.priv = &max_min;
	iter.flags = EBLOB_ITERATE_FLAGS_ALL | EBLOB_ITERATE_FLAGS_READONLY;
	int err = 0;
	err = eblob_iterate(q->eback, &iter);
	if(err != 0)
	{
		return err;
	}
	if(max_min.min != UINT64_MAX)
	{
		q->first_key = max_min.min;
		q->last_key = max_min.max;
		q->length = max_min.max - max_min.min + 1;
	}
	else
	{
		q->first_key = 1;
		q->last_key = 0;
		q->length = 0;
	}
	return 0;
}

int mutex_init(pthread_mutex_t *mutex)
{
	return pthread_mutex_init(mutex, NULL);
}

void mutex_destroy(struct _solid_queue_t *queue)
{
	pthread_mutex_destroy(&(queue->use_queue));
}

int queue_open(struct _solid_queue_t **queue, const queue_param_t queue_param)
{
	if(!queue_param.eblob_param.path)
	{
		return EINVAL;
	}
	struct eblob_log *elog = NULL;
	struct eblob_config *econf = NULL;
	int err = 0;

	if (!(*queue = (struct _solid_queue_t*) malloc (sizeof(struct _solid_queue_t))))
	{
		return ENOMEM;
	}

	if((err = eblob_log_init(&elog, queue_param.eblob_param.log_level, queue_param.eblob_param.priv,
								queue_param.eblob_param.log_h)) != 0 ||
	   (err = eblob_config_init(&econf, queue_param.eblob_param, elog)) != 0 ||
	   !((*queue)->eback = eblob_init(econf)) ||
	   (err = mutex_init(&((*queue)->use_queue))) != 0 ||
	   (err = iterate_queue(*queue, iterator_h, elog)) != 0 ||
	   (err = sem_init(&((*queue)->lock_on_empty), 1, (*queue)->length)) != 0)
	{
		mutex_destroy(*queue);
		if(elog)
			free(elog);
		if(econf)
			eblob_config_free(econf);
		if(*queue)
			free(*queue);
		return (err != 0) ? err : errno;
	}
	(*queue)->count_remaining = ((*queue)->last_key == 0) ? queue_param.max_queue_length :
							 (queue_param.max_queue_length - (*queue)->last_key +
							 (*queue)->first_key - 1);
	(*queue)->time_to_wait = queue_param.time_to_wait;
	eblob_config_free(econf);
	return 0;
}

int push_data(struct _solid_queue_t* queue, void* data, uint64_t len)
{
	++queue->last_key;
	struct eblob_key k;
	memset(&k, 0, sizeof(k));
	memcpy(&k, &(queue->last_key), sizeof(queue->last_key));
	int err = 0;
	if((err = eblob_write(queue->eback, &k, data, 0, (uint64_t)len, 0)) != 0)
	{
		--queue->last_key;
		return err;
	}
	--queue->count_remaining;
	++queue->length;
	return 0;
}

int push_with_displacement(struct _solid_queue_t* queue, void* data, uint64_t len, bool* was_overwrite)
{
	void *d = NULL;
	uint64_t l = 0;
	int err = 0;
	struct eblob_key k;
	memset(&k, 0, sizeof(k));
	memcpy(&k, &(queue->first_key), sizeof(queue->first_key));
	if((err = eblob_read_data(queue->eback, &k, 0, (char**) &d, &l)) != 0)
	{
		return err;
	}
	else
	{
		err = eblob_remove(queue->eback, &k);
		if(err != 0)
		{
			return err;
		}
		++queue->first_key;
		++queue->count_remaining;
		--queue->length;
		if(d) free(d);
	}

	if((err = push_data(queue, data, len)) != 0)
	{
		return err;
	}
	*was_overwrite = 1;
	return 0;
}

int set_timeout(struct timespec* ts, int sec)
{
	if(!ts)
	{
		return -EFAULT;
	}
	if(clock_gettime(CLOCK_REALTIME, ts) == -1)
	{
		return errno;
	}
	ts->tv_sec += sec;

	return 0;
}

int queue_push(struct _solid_queue_t* queue, void* data, uint64_t len, bool *was_overwrite)
{
	struct timespec ts;
	int err = 0;
	int thread_err = 0;
	*was_overwrite = 0;
	if((err = set_timeout(&ts, queue->time_to_wait)) != 0)
	{
		return err;
	}
	if(!queue || !queue->eback)
	{
		return -EFAULT;
	}
	if((thread_err = pthread_mutex_timedlock(&(queue->use_queue), &ts)) != 0)
	{
		return thread_err;
	}
	err = 0;
	if(queue->count_remaining == 0)
	{
		err = push_with_displacement(queue, data, len, was_overwrite);
	}
	else
	{
		err = push_data(queue, data, len);
	}
	if(err == 0 && *was_overwrite == 0 && sem_post(&(queue->lock_on_empty)) == -1)
	{
		return errno;
	}
	if((thread_err = pthread_mutex_unlock(&(queue->use_queue))) != 0)
	{
		return thread_err;
	}
	return err;
}

int queue_pull(struct _solid_queue_t* queue, void **data, uint64_t *len)
{
	struct timespec ts;
	int err = 0;
	int thread_err = 0;
	if((err = set_timeout(&ts, queue->time_to_wait)) != 0)
	{
		return err;
	}
	if(!queue || !data)
	{
		return -EFAULT;
	}
	if(sem_timedwait(&(queue->lock_on_empty), &ts) == -1)
	{
		return errno;
	}
	if((err = pthread_mutex_timedlock(&(queue->use_queue), &ts)) != 0)
	{
		if(sem_post(&(queue->lock_on_empty)) == -1)
		{
			return errno;
		}
		return err;
	}
	*data = NULL;
	*len = 0;
	struct eblob_key k;
	memset(&k, 0, sizeof(k));
	memcpy(&k, &(queue->first_key), sizeof(queue->first_key));
	if((err = eblob_read_data(queue->eback, &k, 0,(char**) data, len)) == 0)
	{
		err = eblob_remove(queue->eback, &k);
		if(err != 0)
		{
			if(sem_post(&(queue->lock_on_empty)) != 0)
			{
				return errno;
			}
			if((thread_err = pthread_mutex_unlock(&(queue->use_queue))) != 0)
			{
				return thread_err;
			}
			return err;
		}
		++queue->first_key;
		++queue->count_remaining;
		--queue->length;
	}
	else
	{
		if(sem_post(&(queue->lock_on_empty)) != 0)
		{
			return errno;
		}
		if((thread_err = pthread_mutex_unlock(&(queue->use_queue))) != 0)
		{
			return thread_err;
		}
		return err;
	}
	if((thread_err = pthread_mutex_unlock(&(queue->use_queue))) != 0)
	{
		return thread_err;
	}
	return 0;
}

void queue_close(struct _solid_queue_t *queue)
{
	if(queue && queue->eback)
	{
		sem_destroy(&(queue->lock_on_empty));
		mutex_destroy(queue);
		eblob_cleanup(queue->eback);
		free(queue);
	}
	return;
}

uint64_t queue_length(struct _solid_queue_t *queue)
{
	return queue->length;
}
