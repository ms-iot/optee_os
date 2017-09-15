//
// Copyright (C) Microsoft. All rights reserved
//

#ifndef __PTA_CYREP_H
#define __PTA_CYREP_H

/*
 * Interface to the cyrep pseudo-TA.
 */

// {5818F318-1887-468F-883A-A8BF7B289B74}
#define PTA_CYREP_UUID { 0x5818f318, 0x1887, 0x468f, { \
		   0x88, 0x3a, 0xa8, 0xbf, 0x7b, 0x28, 0x9b, 0x74 } }
 
enum PTA_CYREP_CMDS {
	//
	// Get the calling TA's private key.
	//
	// [inout] memref[0]: Pointer to CyrepKeyPair structure
	//
	PTA_CYREP_GET_PRIVATE_KEY = 1,

	//
	// Get the calling TA's CYREP certificate chain.
	//
	// [inout] memref[0]: Buffer which will receive the calling TA's
	//                    certificate chain. Call this first with an
	//                    empty buffer to get the required size of
	//                    the buffer.
	//
	PTA_CYREP_GET_CERT_CHAIN,
};


#endif /* __PTA_CYREP_H */
