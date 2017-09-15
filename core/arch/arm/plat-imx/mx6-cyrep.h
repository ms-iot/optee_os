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
#ifndef __MX6_CYREP_H__
#define __MX6_CYREP_H__

/* Cyrep is located in secure RAM bank 2-3 */
#define CYREP_CERT_CHAIN_ADDR          0x00102000
#define CYREP_CERT_CHAIN_SIZE          0x2000

/*
 * The maximum depth from root cert to leaf cert.
 *
 * Root - 4
 *   Device (SPL) - 3
 *     OPTEE - 2
 *       TA - 0
 *       U-Boot - 1
 *         UEFI - 0
 *
 */
#define CYREP_CERT_CHAIN_PATH_LENGTH   4

/* Address at which the next firmware stage's private key is placed */
#define CYREP_KEY_PAIR_ADDR            0x00101000

/* Address where OPTEE's private key is placed */
#define CYREP_OPTEE_KEY_PAIR_ADDR \
	(&((CyrepKeyPair *)CYREP_KEY_PAIR_ADDR)[1])

#endif /* __MX6_CYREP_H__ */

