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
#ifndef __RPMSG_IPC_H__
#define __RPMSG_IPC_H__

#include <stdbool.h>
#include <types_ext.h>

struct rpmsg_ipc_params {
	void (*rx_ipi_handler)(uint32_t ch);
};

bool rpmsg_ipc_boot_remote(void);
bool rpmsg_ipc_wait_remote_ready(uint32_t timeout_ms);
bool rpmsg_ipc_lock_remote(void);
bool rpmsg_ipc_init(struct rpmsg_ipc_params params);
void rpmsg_ipc_rx_ipi_enable(void);
void rpmsg_ipc_rx_ipi_disable(void);
void rpmsg_ipc_ipi_enable(void);
void rpmsg_ipc_ipi_disable(void);
bool rpmsg_ipc_send_msg(uint32_t data);

#endif /* __RPMSG_IPC_H__ */