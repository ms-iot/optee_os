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

enum PTA_RPMSG_CMD {
	//
	// Performs a write followed by read IO on the remote. It is up to the
	// remote and the PTA caller to interpret the content of both the input and
	// output buffers. Their meaning and format is not of an interest to OPTEE
	// or the PTA.
	//
	// [in] value[0].a: A unique value that identifies the CMD and/or sender.
	// [in] memref[0]: An input buffer that gets passed to and read by the remote.
	// [out] memref[0]: An output buffer that gets written by the remote and
	//                  returned back to the caller.
	//
	PTA_RPMSG_CMD_REMOTE_IO,

	//
	// Boots the remote core using a pre-loaded firmware binary.
	//
	// [in] value[0].a: The remote core ID which is system specific.
	//
	PTA_RPMSG_CMD_BOOT_REMOTE
};

#endif /* __PTA_RPMSG_H */
