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

/**
 * @file solid_queue.h
 * @author Dmitry Rudnev
 * @date 19 oct 2014
 * @brief File contains stuctures and definitions of functions, providing queue
 *        interface of disk storage.
 */
#ifndef SOLID_QUEUE_H
#define SOLID_QUEUE_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>
#include <eblob/blob.h>

typedef struct _solid_queue_t solid_queue_t;

/**
 * @brief Eblob configuration parameters.
 * Solidqueue uses eblob library for saving data on disk.
 * These parameters need to configure eblob storage.
 * For more information about eblob parameters visit the website reverbrain.com.
 * @see http://github.com/reverbrain/eblob
 * @see http://doc.reverbrain.com/eblob:api:c_structures#eblob_config
 */
typedef struct _eblob_param_t
{
	unsigned int blob_flags;
	uint64_t blob_size_limit;
	uint64_t blob_size;
	uint64_t records_in_blob;
	int sync;
	int defrag_timeout;
	int defrag_percentage;
	int log_level;
	char *file;
} eblob_param_t;


/**
 * @brief Queue configuration parameters.
 */
typedef struct _queue_param_t
{
	eblob_param_t eblob_param;  /** Eblob configuration parameters. */
	uint64_t num_of_records;    /**< Common number of records in queue.*/
	int time_to_wait;           /**< Timer for semaphore and mutex timedlock. */
} queue_param_t;

/**
 * @brief Function returns current thread id.
 * @return Id of thread, where the function was called.
 */
uint64_t get_thread_id();
/**
 * @brief Function opens existing queue or creates the one, if it doesn't exist.
 * To open or create queue, specify the path in eblob configuration parameter: file.
 * @param queue_param Queue configuration parameters.
 * @return Pointer to queue structure.
 * @see eblob_param_t
 */
solid_queue_t* queue_open(const queue_param_t queue_param);
/**
 * @brief Function pushes data into the queue.
 * If queue is full, the first record wiil be removed and data will
 * be pushed into the queue. @p was_overwrite will return true in this case.
 * @param [in] queue Pointer to queue.
 * @param [in] data Pointer to data.
 * @param [in] len Size of data in bytes.
 * @param [out] was_overwrite Flag which returns true value when displacement occured.
 * @return 0 if OK, errno otherwise.
 */
int queue_push(solid_queue_t* queue, const void* data, size_t len, bool* was_overwrite);
/**
 * @brief Function pulls data out of the queue.
 * @param [in] queue Pointer to queue.
 * @param [out] data Pointer to pulled out data.
 * @param [out] len Size of pulled out data.
 * @return 0 if OK, errno otherwise.
 */
int queue_pull(solid_queue_t* queue, void **data, uint64_t *len);
/**
 *@brief Function closes queue.
 * Function free all used memory, saves data on disk.
 * @param queue Pointer to queue.
 */
void queue_close(solid_queue_t *queue);
#endif // SOLID_QUEUE_H
