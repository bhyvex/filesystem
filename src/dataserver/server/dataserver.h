/*
 * datasever.h
 *
 * Created on 2015.1.23
 * Author:binyang
 */
#ifndef _FILESYSTEM_DATASERVER_H_
#define _FILESYSTEM_DATASERVER_H_

<<<<<<< Updated upstream
=======
//empty head_pos == tail_pos; full (tail_pos + 1) % size == head_pos
#define Q_FULL(head_pos, tail_pos) (((tail_pos + 1) % D_MSG_BSIZE) == (head_pos))
#define Q_EMPTY(head_pos, tail_pos) ((head_pos) == (tail_pos))

>>>>>>> Stashed changes
#include <pthread.h>
#include "dataserver_comm_handler.h"
#include "dataserver_buff.h"
#include "../structure/vfs_structure.h"
#include "../../tool/message.h"

//many kinds of locks
<<<<<<< Updated upstream
=======
pthread_rwlock_t msg_queue_rw_lock;

struct msg_queue_op
{
	void (*push)(common_msg_t*);
	common_msg_t* (*pop)(void);
};

struct msg_queue
{
	int current_size;
	int head_pos;
	int tail_pos;
	common_msg_t* msg;
};

//tread thread pool as a queue??
struct thread_pool
{
	pthread_t* threads;
	int current_size;
	int head_pos;
	int tail_pos;
};
>>>>>>> Stashed changes

//I hope this structure will make this program more clean
struct data_server_operations
{
<<<<<<< Updated upstream
	//there will be a thread run this function
	void* (*m_cmd_receive)(void * msg_queue_arg);
	void (*m_resolve)(msg_queue_t * msg_queue);
=======
	void* (*m_cmd_receive)(void * msg_queue_arg);
	void (*m_resolve)(struct msg_queue * msg_queue);
>>>>>>> Stashed changes
};

struct data_server//so far it likes super blocks in VFS, maybe it will be difference
{
	unsigned int d_memory_used;
	unsigned int d_memory_free;
	unsigned int d_memory_used_for_filesystem;
	unsigned int d_memory_free_for_filesystem;
	unsigned int d_network_free;
	unsigned int machine_id;

	dataserver_sb_t* d_super_block;
	struct data_server_operations* d_op;

	//buffers
	dataserver_file_t* files_buffer;

	//maybe need a specific structure
	vfs_hashtable_t* f_arr_buff;

	//not char* but message*, just test for message passing
	//char* m_cmd_buffer;
	struct msg_queue* m_cmd_queue;
	char* m_data_buffer;//associate with number of thread??

	struct thread_pool* t_buffer;
};

typedef struct data_server data_server_t;

extern data_server_t* data_server;

//-----------------define of some function in struct's operations----------------
//about message receive and resolve
void* m_cmd_receive(void * msg_queue_arg);//there will be a thread run this function
void m_resolve(msg_queue_t * msg_queue);

//--------------------------------------------------------------------------------
//about data server
<<<<<<< Updated upstream
data_server_t* alloc_dataserver(total_size_t t_size, int dev_num);
=======
data_server_t* init_dataserver(total_size_t t_size, int dev_num);
>>>>>>> Stashed changes
int get_current_imformation(struct data_server* server_imf);//返回目前数据节点的信息
void destory_datasrever();

//this is the ultimate goal!
void datasecer_run();
#endif
