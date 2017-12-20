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
#ifndef __IMX_PL310_H__
#define __IMX_PL310_H__

#define L2CACHE_OP_ENABLE                  1
#define L2CACHE_OP_DISABLE                 2
#define L2CACHE_OP_ENABLE_WRITEBACK        3
#define L2CACHE_OP_DISABLE_WRITEBACK       4
#define L2CACHE_OP_ENABLE_WFLZ             5

int l2cache_op(int operation);
int l2cache_enable(void);
int l2cache_disable(void);
int l2cache_enable_writeback(void);
int l2cache_disable_writeback(void);
int l2cache_enable_wflz(void);

#endif /* __IMX_PL310_H__ */

