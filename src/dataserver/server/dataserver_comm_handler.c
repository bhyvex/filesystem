/*
 * created on 2015.4.7
 * author: Binyang
 *
 * This file complete functions defined in dastaserver_comm.h
 */
#include <string.h>
#include <mpi.h>
#include "dataserver_comm_handler.h"
#include "../../tool/errinfo.h"
#include "../structure/vfs_structure.h"
#include "../../tool/message.h"

static int read_from_vfs(dataserver_file_t *file, char* buff, size_t count,
		off_t offset, int seqno, int tail, msg_data_t* msg_buff)
{
	int ans;
	//read a block from vfs
	if((ans = vfs_read(file, buff, count, offset)) == -1)
	{
		err_msg("Something wrong when read from data server");
		//send error message to client
		return -1;
	}

	memcpy(msg_buff->data, buff, MAX_DATA_CONTENT_LEN);
	msg_buff->len = count;
	msg_buff->seqno = seqno;
	msg_buff->offset = offset + ans;//I don't know if this operation is right
	//read to the end of the file unexpectedly
	if(ans < count)
		msg_buff->tail = 1;
	msg_buff->tail = tail;
	return ans;
}


/**
 * handle the request from client about reading from a file
 * buff and msg_buff should be allocated properly outside
 */
int m_read_handler(int source, int tag, msg_for_rw_t* file_info, char* buff, void* msg_buff)
{
	int ans = 0, msg_blocks, msg_rest, i, temp_len, temp_ans, tail;
	off_t offset;
	MPI_Status status;

	MPI_Recv(msg_buff, MAX_CMD_MSG_LEN, MPI_CHAR, source, tag, MPI_COMM_WORLD, &status);
	//client do not want to receive message, and server will not provide
	if(*(unsigned short*)msg_buff != MSG_ACC)
	{
		err_msg("The client do not ready to receive message now");
		return -1;
	}

	msg_blocks = file_info->count / MAX_DATA_CONTENT_LEN;
	msg_rest = file_info->count % MAX_DATA_CONTENT_LEN;
	offset = file_info->offset;
	//adjust for tail message
	msg_blocks = msg_blocks + (1 ? 0 : msg_rest);

	for(i = 0; i < msg_blocks - 1; i++)
	{
		if( (temp_ans = read_from_vfs(file_info->file, buff, MAX_DATA_CONTENT_LEN,
				offset, i, 0, msg_buff)) == -1 )
			return -1;

		MPI_Send(msg_buff, MAX_DATA_MSG_LEN, MPI_CHAR, source, tag, MPI_COMM_WORLD);

		//already read at the end of the file
		if(temp_ans == 0)
			return ans;

		offset = offset + temp_ans;
		ans = ans + temp_ans;
	}

	//send tail message
	if(msg_rest == 0)
		if( (temp_ans = read_from_vfs(file_info->file, buff, MAX_DATA_CONTENT_LEN,
						offset, i, 1, msg_buff)) == -1 )
			return -1;
	else
		if( (temp_ans = read_from_vfs(file_info->file, buff, msg_rest,
						offset, i, 1, msg_buff)) == -1 )
			return -1;

	MPI_Send(msg_buff, MAX_DATA_MSG_LEN, MPI_CHAR, source, tag, MPI_COMM_WORLD);
	ans = ans + temp_ans;

	//maybe there is need to receive Acc from client
	return ans;
}

//handle the request from client about writing to a file
static int write_to_vfs(dataserver_file_t *file, char* buff, off_t offset,
		msg_data_t* msg_buff)
{
	int ans, len;
	len = msg_buff->len;
	//received message from client
	//if we need seq number to keep message in order??
	memcpy(buff, ((msg_data_t *)msg_buff)->data, len);
	//write to file system
	if((ans = vfs_write(file, buff, len, offset)) == -1)
	{
		err_msg("Something wrong when read from data server");
		//send error message to client
		return -1;
	}
	return ans;
}

int m_write_handler(int source, int tag, msg_for_rw_t* file_info, char* buff, void* msg_buff)
{
	int ans = 0, msg_blocks, msg_rest, i, temp_len, temp_ans;
	off_t offset;
	MPI_Status status;


	MPI_Recv(msg_buff, MAX_CMD_MSG_LEN, MPI_CHAR, source, tag, MPI_COMM_WORLD, &status);
	if(*(unsigned short*)msg_buff != MSG_ACC)
	{
		err_msg("The client do not ready to send message now");
		//send error message to client
		return -1;
	}

	msg_blocks = file_info->count / MAX_DATA_CONTENT_LEN;
	msg_rest = file_info->count % MAX_DATA_CONTENT_LEN;
	offset = file_info->offset;

	msg_blocks = msg_blocks + (1 ? 0 : msg_rest );

	for(i = 0; i < msg_blocks; i++)
	{
		//received message from client
		//if we need seq number to keep message in order??
		MPI_Revc(msg_buff, MAX_DATA_MSG_LEN, source, tag, MPI_COMM_WORLD);
		if((temp_ans = write_to_vfs(file_info->file, buff, offset, msg_buff)) == -1)
			return -1;

		offset = offset + temp_ans;
		ans = ans + temp_ans;
	}

	//send tail message
	ans = ans + temp_ans;

	//maybe there is need to send Acc to client
	return ans;
}
