/** @file
*
*  Copyright (c) Microsoft Corporation. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#ifndef __PTA_RPMSG_H
#define __PTA_RPMSG_H

/*
 * Interface to the RPMsg pseudo-TA.
 */

// {616eeba1-faaa-46a7-9faf-d8b84fda1a6b}
#define PTA_RPMSG_UUID { 0x616eeba1, 0xfaaa, 0x46a7, { \
		   0x9f, 0xaf, 0xd8, 0xb8, 0x4f, 0xda, 0x1a, 0x6b } }

#define PTA_RPMSG_UUID_STR "{616eeba1-faaa-46a7-9faf-d8b84fda1a6b}"

enum PTA_RPMSG_CMD {
	//
	// Sends a buffer to remote.
	// If the send queue is full, the CMD will block until a space is available.
	//
	// [in] value[0].a: A unique value (cookie) that identifies the sender.
	// [in] memref[0]: An input buffer that gets sent to the remote.
	//
	// Returns:
	// TEE_ERROR_EXCESS_DATA: If the sent buffer is larger than the rpmsg max
	// payload size defined as RL_BUFFER_PAYLOAD_SIZE which is 496 bytes.
	// TEE_ERROR_COMMUNICATION: If a protocol error occurred during the send
	// operation.
	// TEE_ERROR_BAD_PARAMETERS: If the param types are unexpected.
	//
	PTA_RPMSG_CMD_SEND,

	//
	// Receive a buffer from remote.
	//
	// [in] value[0].a: A unique value (cookie) that identifies the receiver.
	// [inout] memref[0]: An output buffer to which the data received from remote
	// is written to.
	//
	// Returns:
	// TEE_ERROR_NO_DATA: If the there is no outstanding data received from the
	// remote.
	// TEE_ERROR_SHORT_BUFFER: If the provided buffer is too small to hold the
	// received data. The received data will be consumed and gone from the
	// master queue in this situation.
	// TEE_ERROR_BAD_PARAMETERS: If the param types are unexpected.
	//
	PTA_RPMSG_CMD_RECEIVE,
};

#endif /* __PTA_RPMSG_H */
