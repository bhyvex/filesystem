/*
 * data_servers.h
 *
 *  Created on: 2015年5月12日
 *      Author: ron
 */

#include "../../structure/data_server_location.h"
#include "../../structure/basic_list.h"
#ifndef SRC_MAIN_DATA_SERVERS_H_
#define SRC_MAIN_DATA_SERVERS_H_

typedef enum data_server_status{

}data_server_status;


typedef struct master_data_server{
	data_server_status status;
	int server_id;
	unsigned int used_block;
	unsigned int free_block;
	time_t last_update;
}master_data_server;

typedef struct data_server_opera{
	int (*heart_blood)(data_servers *servers, int index);
	list_t *(*file_allocate_machine)(data_servers *servers, unsigned long file_size, int block_size);
}data_server_opera;

typedef struct data_servers{
	unsigned int servers_count;
	unsigned long global_id;
	double load_factor;
	master_data_server *server_list;
	data_server_opera *opera;
}data_servers;

data_servers *data_servers_create(int server_count, double load_factor);

#endif /* SRC_MAIN_DATA_SERVERS_H_ */
