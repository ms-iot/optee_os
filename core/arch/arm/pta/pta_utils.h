/*
 * Copyright (C) Microsoft. All rights reserved
 */

#ifndef PTA_UTILS_H
#define PTA_UTILS_H

/*
 * Auxiliary macros
 */

#define UNREFERENCED_PARAMETER(p) (p) = (p)

#define FIELD_OFFSET(type, field) ((uint32_t)&(((type *)0)->field))
#define FIELD_SIZE(type, field) (sizeof(((type *)0)->field))

/*
 * Stopwatch
 */

struct _stopwatch {
	TEE_Time start_time;   
};
typedef struct _stopwatch stopwatch;

void stopwatch_start(stopwatch* sw);
uint32_t stopwatch_elapsed_millis(stopwatch* sw);

#endif /* PTA_UTILS_H */
