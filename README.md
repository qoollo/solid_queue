Solid Queue Library
===================

About
-----
Solid_queue is a queue with persistent disk storage written in C.
It uses [eblob](https://github.com/reverbrain/eblob) library as backend and provides push/pull interface.
Queue has finite size and if more data come to queue the oldest one will be pushed out from queue.

Usage
-----
### Init
```c
queue_param_t queue_param;
queue_param.eblob_param.blob_size_limit = 1500000000;
queue_param.eblob_param.blob_size = 200000000;
queue_param.eblob_param.records_in_blob = queue_param.eblob_param.blob_size/14000;
queue_param.eblob_param.sync = 5;
queue_param.eblob_param.defrag_timeout = 12;
queue_param.eblob_param.defrag_percentage = 25;
queue_param.eblob_param.blob_flags = EBLOB_TIMED_DATASORT;
queue_param.eblob_param.path = argv[++arg_index];
queue_param.time_to_wait = 5;
queue_param.max_queue_length = 20000;

if(!(queue = queue_open(queue_param)))
{
	exit(1);
}
```
### Usage
Pull from queue:
```c
uint64_t len = 0; // size of data from queue - will be set
void *data; // data pulled from queue
queue_pull(queue, &data, &len)
```
Push to queue:
```c
uint64_t len = 100; // size of data to write
char data[100]; // data to write
bool was_overwrite = false; // if we run out of items free in queue element could be push out
queue_push(queue, data, len, &was_overwrite)
```

### Finalize
```c
queue_close(param->queue);
```

References
------
 * EBLOB project https://github.com/reverbrain/eblob
 * Repository to get binary packages of eblob library for your OS: http://repo.reverbrain.com/
