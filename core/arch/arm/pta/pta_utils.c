/*
 * Copyright (C) Microsoft. All rights reserved
 */

#include <arm.h>
#include <kernel/panic.h>
#include <kernel/tee_time.h>
#include <kernel/tee_time.h>
#include <utee_defines.h>

#include "pta_utils.h"

void stopwatch_start(stopwatch* sw)
{
    if (tee_time_get_sys_time(&sw->start_time) != TEE_SUCCESS)
        panic();
}

uint32_t stopwatch_elapsed_millis(stopwatch* sw)
{
    TEE_Time curr_time;
    TEE_Time elpased_time;
    
    if (tee_time_get_sys_time(&curr_time) != TEE_SUCCESS)
        panic();
    
    TEE_TIME_SUB(curr_time, sw->start_time, elpased_time);
    
    return elpased_time.seconds * 1000L + elpased_time.millis;
}
