/*
 * data_server.c
 *
 *  Created on: 2015年5月12日
 *      Author: ron
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "master_data_servers.h"
#include "../tool/message.h"
#include "../tool/errinfo.h"

/*===============private functions===============*/
//static double load_situation(master_data_server *);
//static void free_file_machine_location(file_machine_location *);
//static int heart_blood(data_servers *servers, int server_id, data_server_status status);
//static int init_data_server(master_data_server *server, unsigned int server_id, unsigned int block_size);
//
//static void block_dup(void *dest, void *source);

/*================private functions implementation===============*/

static int heart_blood(data_servers *servers, int server_id, data_server_status status, time_t time){
	if(servers == NULL || server_id > servers->servers_count){
		err_ret("MASTER_DATA_SERVERS.C: ERROR server is null or server id is illegal %d %d", server_id, servers->servers_count);
		return -1;
	}

	(servers->server_list + server_id)->last_update = time;
	(servers->server_list + server_id)->status = status;
	//err_ret("heart blood from machine %d", server_id);
	return 0;
}

/*initialize one data server machine*/
static int init_data_server(master_data_server *server, unsigned int server_id, unsigned int block_size){
	server->used_block = 0;
	server->free_block = block_size;
	server->server_id = server_id;
	server->status = SERVER_UNINITIAL;
	return 0;
}

static void block_dup(void *dest, void *source){
	memcpy(dest, source, sizeof(block_location));
}

/**
 * allocate storage space for the file according to the file_size
 */
basic_queue_t *file_allocate_machine(data_servers *servers, unsigned long file_size, int block_size){
	int tmp_block_num = ceil((double)file_size / block_size);
	int block_num = tmp_block_num;
	basic_queue_t *queue = alloc_basic_queue(sizeof(block_location), block_num);
	if(block_num == 0){
		return queue;
	}
	queue_set_dup_method(queue, block_dup);
	block_location tmp;
	int seq = 0;
	int count = 0;
	int server_index = 1;
	int j;
	master_data_server *ptr;
	for(; server_index <= servers->servers_count; server_index++){
		ptr = servers->server_list + server_index - 1;
		if(ptr->status != SERVER_AVAILABLE || ptr->free_block == 0){
			continue;
		}

		if(ptr->free_block > block_num){
			count = block_num;
			block_num = 0;
			ptr->free_block -= block_num;
		}else{
			count = ptr->free_block;
			block_num -= count;
			ptr->free_block = 0;
		}
		for(j = 0; j != count; j++){
			tmp.block_seq = seq++;
			tmp.global_id = servers->global_id++;
			tmp.server_id = server_index;
			tmp.write_len = block_size;
			queue->basic_queue_op->push(queue, &tmp);
		}

		if(block_num == 0){
			((block_location *)(queue->elements + (queue->current_size - 1) * queue->element_size))->write_len = file_size - (tmp_block_num - 1) * block_size;
			break;
		}
	}

	if(block_num == 0){
		//printf("sasjalfmlamcoa0912e1jmn");
		return queue;
	}
	else{
		basic_queue_iterator *iterator = create_basic_queue_iterator(queue);
		while(iterator != NULL && iterator->has_next(iterator)){
			iterator->next(iterator, &tmp);
			servers->global_id--;
			(servers->server_list + tmp.server_id)->free_block++;
			(servers->server_list + tmp.server_id)->used_block--;
			//file_machine_location
		}
		free(iterator);
		destroy_basic_queue(queue);
		return NULL;
	}
}

data_servers *data_servers_create(unsigned int server_count, double load_factor, unsigned int server_block_size, unsigned int server_block_num){
	data_servers *servers = (data_servers *)malloc(sizeof(data_servers));
	if(servers == NULL){
		return NULL;
	}

	servers->server_list = (master_data_server*)malloc(sizeof(master_data_server) * server_count);
	if(servers->server_list == NULL){
		free(servers);
		return NULL;
	}

	int i;
	//initialize each data server machine
	for(i = 0; i != server_count; i++){
		init_data_server(servers->server_list + i, i, server_block_num);
	}

	servers->opera = (data_server_opera *)malloc(sizeof(data_server_opera));
	if(servers->opera == NULL){
		free(servers);
		free(servers->server_list);
		return NULL;
	}

	servers->global_id = 0;
	servers->load_factor = load_factor;
	servers->opera->heart_blood = heart_blood;
	servers->opera->file_allocate_machine = file_allocate_machine;
	servers->servers_count = server_count;
	servers->server_block_size = server_block_size;
	servers->server_block_num = server_block_num;
	return servers;
}

int data_servers_destroy(data_servers * servers){
	if(servers != NULL)
	{
		free(servers->opera);
		free(servers->server_list);
		free(servers);
	}
	return 0;
}

