//
// Copyright (C) Microsoft. All rights reserved
//

#ifndef __PTA_HELLO_WORLD_H
#define __PTA_HELLO_WORLD_H

/*
 * Interface to the hello world pseudo-TA.
 */

/*
 * For testing using TA_HELLO_WORLD_UUID
 */
#define PTA_HELLO_WORLD_UUID { 0x8aaaf200, 0x2450, 0x11e4, { \
                       0xab, 0xe2, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }

//
// Description
// 
// [in] memref[0]: 
//  [in] Input buffer
// [in/out] memref[1]: 
//  [out] Output buffer (input buffer transposed)
// 
#define PTA_HELLO_WORLD_CMD1    0

#endif /* __PTA_HELLO_WORLD_H */
