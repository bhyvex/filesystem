/*
 * rpc_server.c
 *
 *  Created on: 2015年7月10日
 *      Author: ron
 */

#include <math.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "rpc_server.h"
#include "zmalloc.h"
#include "syn_tool.h"
#include "log.h"
#include "basic_list.h"
#include "message.h"

/*--------------------Private Declaration---------------------*/
static void server_start(rpc_server_t *server);
static void server_start2(rpc_server_t *server);
static void server_stop_send_queue(rpc_server_t *server);
static void server_stop(rpc_server_t *server);
static void server_stop2(rpc_server_t *server);
static int send_result(void *param, int dst, int tag, int len, msg_type_t type);
static void send_to_queue(rpc_server_t *server, void *param, int dst, int tag, int len);
static int recv_reply(void* param, int source, int tag, msg_type_t type);
static void* send_msg_from_queue(void* server);

/*--------------------Private Implementation------------------*/
static void server_start(rpc_server_t *server)
{
	server->thread_pool->tp_ops->start(server->thread_pool);

	log_write(LOG_DEBUG, "RPC_SERVER && THREAD POOL START AND ID = %d", server->server_id);
	if(server->send_queue != NULL)
	{
		pthread_create(&server->qsend_tid, NULL, send_msg_from_queue, server);
	}

	//TODO multi_thread access server_thread_cancel may read error status
	while(!server->server_thread_cancel)
	{
		recv_common_msg(server->recv_buff, ANY_SOURCE, CMD_TAG);
#if RPC_SERVER_DEBUG
log_write(LOG_DEBUG, "RPC Server received a cmd and code = %d", ((common_msg_t *)server->recv_buff)->operation_code);
#endif
		if(((common_msg_t *)server->recv_buff)->operation_code == SERVER_STOP)
		{
			server_stop(server);
		}
		else
		{
			server->request_queue->op->syn_queue_push(server->request_queue, server->recv_buff);
		}
	}
	//no more message need to send
	server_stop_send_queue(server);
}

static void server_start2(rpc_server_t *server)
{
	server->thread_pool->tp_ops->start(server->thread_pool);

	log_write(LOG_DEBUG, "RPC_SERVER && THREAD POOL START");
	if(server->send_queue != NULL)
	{
		pthread_create(&server->qsend_tid, NULL, send_msg_from_queue, server);
	}
}

static void server_stop(rpc_server_t *server)
{
	stop_server_msg_t* stop_server_msg;

	server->server_thread_cancel = 1;
	stop_server_msg = (stop_server_msg_t *)MSG_COMM_TO_CMD(server->recv_buff);
	acc_msg_t *acc_msg = (acc_msg_t*)zmalloc(sizeof(acc_msg_t));

	acc_msg->op_status = ACC_OK;
	server->op->send_result(acc_msg, stop_server_msg->source, stop_server_msg->tag, IGNORE_LENGTH, ACC);
	zfree(acc_msg);
}

static void server_stop_send_queue(rpc_server_t *server)
{
	if(server->send_queue != NULL)
	{
		while(!server->send_queue->queue->basic_queue_op->is_empty(server->send_queue->queue))
		{
			usleep(50);
		}
		pthread_cancel(server->qsend_tid);
		pthread_join(server->qsend_tid, NULL);
		pthread_mutex_unlock(server->send_queue->queue_mutex);
		log_write(LOG_DEBUG, "send queue thread has been canceled");
	}

#if RPC_SERVER_DEBUG
	log_write(LOG_DEBUG, "finished server stop send queue");
#endif
}

static void server_stop2(rpc_server_t *server)
{
	server_stop_send_queue(server);

#if RPC_SERVER_DEBUG
	log_write(LOG_DEBUG, "finished server stop2");
#endif
}

//only data message and ans message need len
static int send_result(void *param, int dst, int tag, int len, msg_type_t type)
{
	head_msg_t head_msg;

	switch(type)
	{
		case ACC:
			send_acc_msg(param, dst, tag, ACC_IGNORE);
			break;
		case DATA:
			send_data_msg(param, dst, tag, len);
			break;
		case ANS:
			head_msg.op_status = ACC_OK;
			head_msg.len = len;

			send_head_msg(&head_msg, dst, tag);
			send_msg(param, dst, tag, head_msg.len);
			break;
		case CMD:
			send_cmd_msg(param, dst, tag);
			break;
		case HEAD:
			send_head_msg(param, dst, tag);
			break;
		default:
			log_write(LOG_ERR, "wrong type of message, check your code");
			return -1;
	}
	return 0;
}

static void send_to_queue(rpc_server_t *server, void *param, int dst, int tag, int len)
{
	syn_queue_t *send_queue = server->send_queue;
	rpc_send_msg_t *rpc_send_msg = server->send_buff;
	assert(rpc_send_msg->msg == NULL);
#if RPC_SERVER_DEBUG
	log_write(LOG_DEBUG, "rpc server send to queue");
#endif
	rpc_send_msg->dst = dst;
	rpc_send_msg->tag = tag;
	rpc_send_msg->length = len;
	rpc_send_msg->msg = zmalloc(len);
	memcpy(rpc_send_msg->msg, param, len);
	send_queue->op->syn_queue_push(send_queue, rpc_send_msg);
	rpc_send_msg->msg = NULL;
#if RPC_SERVER_DEBUG
	log_write(LOG_DEBUG, "rpc server send to queue end");
#endif
}

static void clean_up(void *rpc_send_msg)
{
	zfree(rpc_send_msg);
}

static void* send_msg_from_queue(void* server)
{
	rpc_server_t *rpc_server = (rpc_server_t *)server;
	syn_queue_t *send_queue = rpc_server->send_queue;
	rpc_send_msg_t *rpc_send_msg = zmalloc(sizeof(rpc_send_msg_t));

	pthread_cleanup_push(clean_up, rpc_send_msg);
	while(1)
	{
#if RPC_SERVER_DEBUG
	log_write(LOG_DEBUG, "rpc server send message from queue start");
#endif
		send_queue->op->syn_queue_pop(send_queue, rpc_send_msg);
		send_msg(rpc_send_msg->msg, rpc_send_msg->dst, rpc_send_msg->tag, rpc_send_msg->length);
		zfree(rpc_send_msg->msg);
		rpc_send_msg->msg = NULL;
#if RPC_SERVER_DEBUG
	log_write(LOG_DEBUG, "rpc server send message from queue end");
#endif
	}

	pthread_cleanup_pop(0);
	return NULL;
}

static int recv_reply(void* param, int source, int tag, msg_type_t type)
{
	switch(type)
	{
		case ACC:
			recv_acc_msg(param, source, tag);
			break;
		case DATA:
			recv_data_msg(param, source, tag, IGNORE_LENGTH);
			break;
		default:
			log_write(LOG_ERR, "wrong type of message, check your code");
			return -1;
	}
	return 0;
}
/*--------------------API Implementation--------------------*/
rpc_server_t *create_rpc_server(int thread_num, int queue_size,
		int server_id, resolve_handler_t resolve_handler)
{
	rpc_server_t *this = zmalloc(sizeof(rpc_server_t));

	this->request_queue = alloc_syn_queue(queue_size, sizeof(common_msg_t));
	this->send_queue = NULL;
	this->send_buff = NULL;
	this->server_id = server_id;
	this->server_thread_cancel = 0;
	this->thread_pool = alloc_thread_pool(thread_num, this->request_queue, resolve_handler);

	this->op = zmalloc(sizeof(rpc_server_op_t));
	this->op->server_start = server_start;
	this->op->server_start2 = server_start2;
	this->op->server_stop = server_stop;
	this->op->server_stop2 = server_stop2;
	this->op->send_result = send_result;
	this->op->send_to_queue = send_to_queue;
	this->op->recv_reply = recv_reply;

	this->recv_buff = zmalloc(sizeof(common_msg_t));
	return this;
}

rpc_server_t *create_rpc_server2(int thread_num, int recv_qsize, int send_qsize,
		int server_id, resolve_handler_t resolve_handler)
{
	rpc_server_t *this = create_rpc_server(thread_num, recv_qsize, server_id,
			resolve_handler);
	this->send_queue = alloc_syn_queue(send_qsize, sizeof(rpc_send_msg_t));
	this->send_buff = zmalloc(sizeof(rpc_send_msg_t));
	this->send_buff->msg = NULL;

	return this;
}

void destroy_rpc_server(rpc_server_t *server)
{
#if RPC_SERVER_DEBUG
	log_write(LOG_DEBUG, "destroy rpc server start");
#endif

	destroy_thread_pool(server->thread_pool);

#if RPC_SERVER_DEBUG
	log_write(LOG_DEBUG, "destroy rpc server thread pool");
#endif

	destroy_syn_queue(server->request_queue);

#if RPC_SERVER_DEBUG
	log_write(LOG_DEBUG, "destroy rpc server syn queue");
#endif

	if(server->send_queue != NULL){
		destroy_syn_queue(server->send_queue);
	}
	zfree(server->op);
	zfree(server->recv_buff);
	zfree(server->send_buff);
	zfree(server);

#if RPC_SERVER_DEBUG
	log_write(LOG_DEBUG, "free rpc server memory");
#endif
}
