/*
 * Copyright (C) ST-Microelectronics 2013. All rights reserved.
 */
#ifndef _TEE_IOC_H
#define _TEE_IOC_H

#include "linux_nutlet/tee_client_api.h"

#ifndef __KERNEL__
#define __user
#endif

/**
 * struct tee_cmd_io - The command sent to an open tee device.
 * @err: Error code (as in Global Platform TEE Client API spec)
 * @origin: Origin for the error code (also from spec).
 * @cmd: The command to be executed in the trusted application.
 * @uuid: The uuid for the trusted application.
 * @data: The trusted application or memory block.
 * @data_size: The size of the trusted application or memory block.
 * @op: The cmd payload operation for the trusted application.
 *
 * This structure is mainly used in the Linux kernel for communication
 * with the user space.
 */
struct tee_cmd_io {
	TEEC_Result err;
	uint32_t origin;
	uint32_t cmd;
	TEEC_UUID __user *uuid;
	void __user *data;
	uint32_t data_size;
	TEEC_Operation __user *op;
	int fd_sess;
};

struct tee_shm_io {
	void __user *buffer;
	size_t size;
	uint32_t flags;
	int fd_shm;
	uint8_t registered;
};

#define TEE_OPEN_SESSION_IOC		_IOWR('t', 161, struct tee_cmd_io)
#define TEE_INVOKE_COMMAND_IOC		_IOWR('t', 163, struct tee_cmd_io)
#define TEE_REQUEST_CANCELLATION_IOC	_IOWR('t', 164, struct tee_cmd_io)
#define TEE_ALLOC_SHM_IOC		_IOWR('t', 165, struct tee_shm_io)
#define TEE_GET_FD_FOR_RPC_SHM_IOC	_IOWR('t', 167, struct tee_shm_io)

#ifdef CONFIG_COMPAT
struct tee_cmd_io_32 {
	uint32_t err;
	uint32_t origin;
	uint32_t cmd;
	uint32_t uuid;
	uint32_t data;
	uint32_t data_size;
	uint32_t op;
	uint32_t fd_sess;
};

struct tee_shm_io_32 {
	uint32_t buffer;
	uint32_t size;
	uint32_t flags;
	uint32_t fd_shm;
	uint8_t registered;
};

/**
 * struct TEEC_Context - Represents a connection between a client application
 * and a TEE.
 */
typedef struct {
	char devname[256];
	uint32_t fd;
} TEEC_Context_32;

/**
 * This type contains a Universally Unique Resource Identifier (UUID) type as
 * defined in RFC4122. These UUID values are used to identify Trusted
 * Applications.
 */
typedef struct {
	uint32_t timeLow;
	uint16_t timeMid;
	uint16_t timeHiAndVersion;
	uint8_t clockSeqAndNode[8];
} TEEC_UUID_32;

/**
 * struct TEEC_SharedMemory - Memory to transfer data between a client
 * application and trusted code.
 *
 * @param buffer      The memory buffer which is to be, or has been, shared
 *                    with the TEE.
 * @param size        The size, in bytes, of the memory buffer.
 * @param flags       Bit-vector which holds properties of buffer.
 *                    The bit-vector can contain either or both of the
 *                    TEEC_MEM_INPUT and TEEC_MEM_OUTPUT flags.
 *
 * A shared memory block is a region of memory allocated in the context of the
 * client application memory space that can be used to transfer data between
 * that client application and a trusted application. The user of this struct
 * is responsible to populate the buffer pointer.
 */
typedef struct {
	uint32_t buffer;
	size_t size;
	uint32_t flags;
	/* Implementation-Defined, must match what the kernel driver have */
	union {
		uint32_t fd;
		uint32_t ptr;
	} d;
	uint8_t registered;
} TEEC_SharedMemory_32;

/**
 * struct TEEC_TempMemoryReference - Temporary memory to transfer data between
 * a client application and trusted code, only used for the duration of the
 * operation.
 *
 * @param buffer  The memory buffer which is to be, or has been shared with
 *                the TEE.
 * @param size    The size, in bytes, of the memory buffer.
 *
 * A memory buffer that is registered temporarily for the duration of the
 * operation to be called.
 */
typedef struct {
	uint32_t buffer;
	uint32_t size;
} TEEC_TempMemoryReference_32;

/**
 * struct TEEC_RegisteredMemoryReference - use a pre-registered or
 * pre-allocated shared memory block of memory to transfer data between
 * a client application and trusted code.
 *
 * @param parent  Points to a shared memory structure. The memory reference
 *                may utilize the whole shared memory or only a part of it.
 *                Must not be NULL
 *
 * @param size    The size, in bytes, of the memory buffer.
 *
 * @param offset  The offset, in bytes, of the referenced memory region from
 *                the start of the shared memory block.
 *
 */
typedef struct {
//	TEEC_SharedMemory *parent;
	uint32_t parent;
	uint32_t size;
	uint32_t offset;
} TEEC_RegisteredMemoryReference_32;

/**
 * struct TEEC_Value - Small raw data container
 *
 * Instead of allocating a shared memory buffer this structure can be used
 * to pass small raw data between a client application and trusted code.
 *
 * @param a  The first integer value.
 *
 * @param b  The second second value.
 */
typedef struct {
	uint32_t a;
	uint32_t b;
} TEEC_Value_32;

/**
 * union TEEC_Parameter - Memory container to be used when passing data between
 *                        client application and trusted code.
 *
 * Either the client uses a shared memory reference, parts of it or a small raw
 * data container.
 *
 * @param tmpref  A temporary memory reference only valid for the duration
 *                of the operation.
 *
 * @param memref  The entire shared memory or parts of it.
 *
 * @param value   The small raw data container to use
 */
typedef union {
	TEEC_TempMemoryReference_32 tmpref;
	TEEC_RegisteredMemoryReference_32 memref;
	TEEC_Value_32 value;
} TEEC_Parameter_32;

/**
 * struct TEEC_Session - Represents a connection between a client application
 * and a trusted application.
 */
typedef struct {
	uint32_t fd;
} TEEC_Session_32;

/**
 * struct TEEC_Operation - Holds information and memory references used in
 * TEEC_InvokeCommand().
 *
 * @param   started     Client must initialize to zero if it needs to cancel
 *                      an operation about to be performed.
 * @param   paramTypes  Type of data passed. Use TEEC_PARAMS_TYPE macro to
 *                      create the correct flags.
 *                      0 means TEEC_NONE is passed for all params.
 * @param   params      Array of parameters of type TEEC_Parameter.
 * @param   session     Internal pointer to the last session used by
 *                      TEEC_InvokeCommand with this operation.
 *
 */
typedef struct {
	uint32_t started;
	uint32_t paramTypes;
	TEEC_Parameter_32 params[TEEC_CONFIG_PAYLOAD_REF_COUNT];
	/* Implementation-Defined */
	uint32_t session;
	TEEC_SharedMemory_32 memRefs[TEEC_CONFIG_PAYLOAD_REF_COUNT];
	uint32_t flags;
} TEEC_Operation_32;


#define TEE_OPEN_SESSION_IOC_32		_IOWR('t', 161, struct tee_cmd_io_32)
#define TEE_INVOKE_COMMAND_IOC_32		_IOWR('t', 163, struct tee_cmd_io_32)
#define TEE_REQUEST_CANCELLATION_IOC_32	_IOWR('t', 164, struct tee_cmd_io_32)
#define TEE_ALLOC_SHM_IOC_32		_IOWR('t', 165, struct tee_shm_io_32)
#define TEE_GET_FD_FOR_RPC_SHM_IOC_32	_IOWR('t', 167, struct tee_shm_io_32)



int tee_cmd_io_32_to_64(struct tee_cmd_io* io_64, struct tee_cmd_io_32* io_32);
int tee_shm_io_32_to_64(struct tee_shm_io* io_64, struct tee_shm_io_32* io_32);
int tee_op_32_to_64(TEEC_Operation* op_64, TEEC_Operation_32* op_32);
int tee_is_ioctl32(struct tee_context * ctx);
void tee_op_shm_32_to_64(TEEC_SharedMemory * sm64, TEEC_SharedMemory_32 * sm32);

#endif



#endif /* _TEE_IOC_H */
