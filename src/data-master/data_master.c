/*
 * data_master.c
 *
 *  Created on: 2015年10月8日
 *      Author: ron
 */

#include "data_master.h"
#include "../common/structure_tool/list_queue_util.h"

/*--------------------Private Declaration------------------*/
static int has_enough_space();
static list_t *allocate_space();
static void create_temp_file();
static void append_temp_file();
static void read_temp_file();
static void delete_temp_file();

static data_master_t *local_master;
/*--------------------API Declaration----------------------*/


/*--------------------Private Implementation---------------*/

static uint32_t hash_code(const sds key, size_t size) {
	size_t len = sds_len(key);
	/* 'm' and 'r' are mixing constants generated offline.
	   They're not really 'magic', they just happen to work well.  */
	uint32_t seed = 5381;
	const uint32_t m = 0x5bd1e995;
	const int r = 24;

	/* Initialize the hash to a 'random' value */
	uint32_t h = seed ^ len;

	/* Mix 4 bytes at a time into the hash */
	const unsigned char *data = (const unsigned char *)key;
	while(len >= 4) {
		uint32_t k = *(uint32_t*)data;
		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
	}
	/* Handle the last few bytes of the input array  */
	switch(len) {
		case 3: h ^= data[2] << 16;
				h ^= data[1] << 8;
				h ^= data[0]; h *= m;
				break;
		case 2: h ^= data[1] << 8;
				h ^= data[0]; h *= m;
				break;
		case 1: h ^= data[0]; h *= m;
	};

	/* Do a few final mixes of the hash to ensure the last few
	 * bytes are well-incorporated. */
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return (uint32_t)h % size;
}

/**
 * Thread pool handler parameter
 */
static void *get_event_handler_param(event_handler_t *event_handler) {
	return event_handler->event_buffer_list->head->value;
}

static int has_enough_space(uint64_t block_num){
	if(block_num < local_master->free_size){
		return 0;
	}else{
		return 1;
	}
}

static list_t *allocate_space(uint64_t allocate_size, int index){
	assert(has_enough_space(allocate_size));

	uint64_t start_global_id = local_master->global_id;
	uint64_t end_global_id = start_global_id + allocate_size;
	local_master->global_id = end_global_id;

	list_t *list = list_create();
	basic_queue_t *queue = local_master->storage_q;
	int end = index;
	int t_index = index;
	storage_machine_sta_t *st;
	do{
		st = get_queue_element(queue, t_index);
		if(!st->free_blocks){
			continue;
		}
		position_des_t *position = zmalloc(sizeof(position_des_t));
		position->rank = st->rank;
		position->start = start_global_id;
		if(st->free_blocks < allocate_size){
			position->end = start_global_id + st->free_blocks - 1;
			start_global_id = position->end + 1;

			allocate_size -= st->free_blocks;
			st->used_blocks += st->free_blocks;
			st->free_blocks = 0;
		}else{
			position->end = start_global_id + allocate_size - 1;
			start_global_id = position->end + 1;

			st->free_blocks -= allocate_size;
			st->used_blocks += allocate_size;
			allocate_size = 0;
			break;
		}
		t_index = (t_index + 1) % queue->current_size;
		list->list_ops->list_add_node_tail(list, position);
	}while(t_index != end && allocate_size != 0);

	t_index = index;
	if(allocate_size == 0){
		return list;
	}else{
		list_iter_t *iter = list->list_ops->list_get_iterator(list, AL_START_HEAD);
		while(list->list_ops->list_has_next(iter)){
			position_des_t *position = ((list_node_t *)list->list_ops->list_next(iter))->value;
			do{
				st = get_queue_element(queue, t_index++);
			}while(st->rank != position.rank);
			int allocated_c = position->end - position->start + 1;
			st->free_blocks += allocated_c;
			st->used_blocks -= allocated_c;
		}
		list->list_ops->list_release_iterator(iter);
		list_release(list);
		return NULL;
	}
}

static void create_temp_file(event_handler_t *event_handler){
	//assert(exists);

	c_d_create_t *c_cmd = get_event_handler_param(event_handler);
	sds file_name = sds_new(c_cmd->file_name);
	assert(!local_master->namespace->op->file_exists(local_master->namespace, file_name));

	local_master->namespace->op->add_temporary_file(file_name);

	//send result
}

static void append_temp_file(event_handler_t *event_handler){

	c_d_append_t *c_cmd = get_event_handler_param(event_handler);
	sds file_name = sds_new(c_cmd->file_name);
	assert(local_master->namespace->op->file_exists(local_master->namespace, file_name));

	int index = hash_code(file_name, local_master->group_size);
	list_t *list = allocate_space(c_cmd->write_size, index);
	assert(list != NULL);

	int size = list->len;
	void *pos_arrray = list_to_array(list, sizeof(position_des_t));
	//TODO free function
	list_release(list);

	local_master->rpc_server->op->send_result(pos_arrray, c_cmd->source, c_cmd->tag, size * sizeof(position_des_t), ANS);
	zfree(pos_arrray);
}

static void read_temp_file(event_handler_t *event_handler){
	//test whether file exists
	//get file position
	//send result to client
}

static void delete_temp_file(event_handler_t *event_handler){
	//well this may be much more difficult
}

static void *resolve_handler(event_handler_t* event_handler, void* msg_queue) {
	common_msg_t common_msg;
	syn_queue_t* queue = msg_queue;
	queue->op->syn_queue_pop(queue, &common_msg);
	switch(common_msg.operation_code)
	{
		case CREATE_TEMP_FILE_CODE:
			event_handler->handler = create_temp_file;
			break;
		case APPEND_TEMP_FILE_CODE:
			event_handler->handler = create_temp_file;
			break;
		case READ_TEMP_FILE_CODE:
			event_handler->handler = create_temp_file;
			break;
		default:
			event_handler->handler = NULL;
	}
	return event_handler->handler;
}

int data_master_init(){

}

int data_master_destroy(){

}
