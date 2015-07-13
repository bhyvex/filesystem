/*
 * mpi_rpc_structure.h
 *
 *  Created on: 2015年7月12日
 *      Author: ron
 */

#ifndef SRC_COMMON_COMMUNICATION_MPI_RPC_STRUCTURE_H_
#define SRC_COMMON_COMMUNICATION_MPI_RPC_STRUCTURE_H_
#include "stdint.h"

struct pre_order {
	 uint16_t order_size;
	 uint16_t operation_code;
	 int source;
	 int tag;
};

typedef struct pre_order pre_order_t;

#endif /* SRC_COMMON_COMMUNICATION_MPI_RPC_STRUCTURE_H_ */