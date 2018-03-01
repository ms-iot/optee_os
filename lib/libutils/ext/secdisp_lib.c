/*
* Copyright (C) Microsoft. All rights reserved
*/

#include <printk.h>
#include <stdarg.h>
#include <string.h>

#include "secdisp_lib.h"

static TEE_TASessionHandle secdisp_pta_handle = NULL;
static SECDISP_INFORMATION secdisp_info;

void delay(uint32_t msec);

TEE_Result secdisp_init(SECDISP_DRIVERS driver, SECDISP_INFORMATION* disp_info)
{
    static const TEE_UUID secdisp_pta_uuid = PTA_SECDISP_UUID;
    TEE_Result status;
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;

    status = TEE_OpenTASession(
        &secdisp_pta_uuid,
        0,          // cancellationRequestTimeout
        0,          // paramTypes
        NULL,       // params
        &secdisp_pta_handle,
        NULL);      // returnOrigin

    if (status != TEE_SUCCESS) {
        EMSG("SECDISP open session failed, status %d!", status);
        return status;
    }

    memset(params, 0, sizeof(params));
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_MEMREF_OUTPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    params[0].value.a = driver;
    params[1].memref.buffer = disp_info;
    params[1].memref.size = sizeof(*disp_info);

    status = TEE_InvokeTACommand(
        secdisp_pta_handle,
        0,          // cancellationRequestTimeout
        PTA_SECDISP_INIT,
        param_types,
        params,
        NULL);     // returnOrigin    

    if (status != TEE_SUCCESS) {
        EMSG("PTA_SECDISP_INIT failed, status %d!", status);
        goto done;
    }

    memcpy(&secdisp_info, disp_info, sizeof(*disp_info));

done:

    if (status != TEE_SUCCESS) {
        TEE_CloseTASession(secdisp_pta_handle);
        secdisp_pta_handle = NULL;
    }

    return status;
}

void secdisp_deinit(void)
{
    if (secdisp_pta_handle != NULL) {
        TEE_CloseTASession(secdisp_pta_handle);
        secdisp_pta_handle = NULL;
    }
}

TEE_Result secdisp_clear(SECDISP_COLORS color)
{
    TEE_Result status;
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;

    if (secdisp_pta_handle == NULL) {
        return TEE_ERROR_BAD_STATE;
    }

    memset(params, 0, sizeof(params));
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    params[0].value.a = color;

    status = TEE_InvokeTACommand(
        secdisp_pta_handle,
        0,
        PTA_SECDISP_CLEAR,
        param_types,
        params,
        NULL);

    if (status != TEE_SUCCESS) {
        EMSG("PTA_SECDISP_CLEAR failed, status %d!", status);
        return status;
    }

    return TEE_SUCCESS;
}

TEE_Result secdisp_draw_pixel(int16_t x, int16_t y, SECDISP_COLORS color)
{
    TEE_Result status;
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;

    if (secdisp_pta_handle == NULL) {
        return TEE_ERROR_BAD_STATE;
    }

    memset(params, 0, sizeof(params));
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    params[0].value.a = x;
    params[0].value.b = y;
    params[1].value.a = color;

    status = TEE_InvokeTACommand(
        secdisp_pta_handle,
        0,
        PTA_SECDISP_DRAW_PIXEL,
        param_types,
        params,
        NULL);

    if (status != TEE_SUCCESS) {
        EMSG("PTA_SECDISP_DRAW_PIXEL failed, status %d!", status);
        return status;
    }

    return TEE_SUCCESS;
}

TEE_Result secdisp_draw_line(
    int16_t x,
    int16_t y,
    int16_t size,
    SECDISP_COLORS color,
    bool is_vertical)
{
    TEE_Result status;
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;

    if (secdisp_pta_handle == NULL) {
        return TEE_ERROR_BAD_STATE;
    }

    memset(params, 0, sizeof(params));
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE);

    params[0].value.a = x;
    params[0].value.b = y;
    params[1].value.a = size;
    params[2].value.a = color;

    status = TEE_InvokeTACommand(
        secdisp_pta_handle,
        0,
        is_vertical ? PTA_SECDISP_DRAW_VLINE : PTA_SECDISP_DRAW_HLINE,
        param_types,
        params,
        NULL);

    if (status != TEE_SUCCESS) {
        EMSG("PTA_SECDISP_DRAW_VLINE/PTA_SECDISP_DRAW_HLINE failed, status %d!", status);
        return status;
    }

    return TEE_SUCCESS;
}

TEE_Result secdisp_fill_rect(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h,
    SECDISP_COLORS color)
{
    TEE_Result status;
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;

    if (secdisp_pta_handle == NULL) {
        return TEE_ERROR_BAD_STATE;
    }

    memset(params, 0, sizeof(params));
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE);

    params[0].value.a = x;
    params[0].value.b = y;
    params[1].value.a = w;
    params[1].value.b = h;
    params[2].value.a = color;

    status = TEE_InvokeTACommand(
        secdisp_pta_handle,
        0,
        PTA_SECDISP_FILL_RECT,
        param_types,
        params,
        NULL);

    if (status != TEE_SUCCESS) {
        EMSG("PTA_SECDISP_FILL_RECT failed, status %d!", status);
        return status;
    }

    return TEE_SUCCESS;
}

TEE_Result secdisp_set_rotation(SECDISP_ROTATION rotation)
{
    TEE_Result status;
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;

    if (secdisp_pta_handle == NULL) {
        return TEE_ERROR_BAD_STATE;
    }

    memset(params, 0, sizeof(params));
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    params[0].value.a = rotation;

    status = TEE_InvokeTACommand(
        secdisp_pta_handle,
        0,
        PTA_SECDISP_SET_ROTATION,
        param_types,
        params,
        NULL);

    if (status != TEE_SUCCESS) {
        EMSG("PTA_SECDISP_SET_ROTATION failed, status %d!", status);
        return status;
    }

    return TEE_SUCCESS;
}

TEE_Result secdisp_invert_display(bool is_invert)
{
    TEE_Result status;
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;

    if (secdisp_pta_handle == NULL) {
        return TEE_ERROR_BAD_STATE;
    }

    memset(params, 0, sizeof(params));
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    params[0].value.a = is_invert;

    status = TEE_InvokeTACommand(
        secdisp_pta_handle,
        0,
        PTA_SECDISP_INVERT_DISPLAY,
        param_types,
        params,
        NULL);

    if (status != TEE_SUCCESS) {
        EMSG("PTA_SECDISP_INVERT_DISPLAY failed, status %d!", status);
        return status;
    }

    return TEE_SUCCESS;
}

TEE_Result secdisp_set_text_attr(
    SECDISP_COLORS text_color,
    SECDISP_COLORS background_color,
    uint16_t text_size)
{
    TEE_Result status;
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;

    if (secdisp_pta_handle == NULL) {
        return TEE_ERROR_BAD_STATE;
    }

    memset(params, 0, sizeof(params));
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    params[0].value.a = text_color;
    params[0].value.b = background_color;
    params[1].value.a = text_size;

    status = TEE_InvokeTACommand(
        secdisp_pta_handle,
        0,
        PTA_SECDISP_SET_TEXT_ATTR,
        param_types,
        params,
        NULL);

    if (status != TEE_SUCCESS) {
        EMSG("PTA_SECDISP_SET_TEXT_ATTR failed, status %d!", status);
        return status;
    }

    return TEE_SUCCESS;
}

TEE_Result secdisp_write_text(
    int16_t x,
    int16_t y,
    const char *text,
    uint16_t count)
{
    TEE_Result status;
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;

    if (secdisp_pta_handle == NULL) {
        return TEE_ERROR_BAD_STATE;
    }

    memset(params, 0, sizeof(params));
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    params[0].value.a = x;
    params[0].value.b = y;
    params[1].memref.buffer = (void*)text;
    params[1].memref.size = count;

    status = TEE_InvokeTACommand(
        secdisp_pta_handle,
        0,
        PTA_SECDISP_WRITE_TEXT,
        param_types,
        params,
        NULL);

    if (status != TEE_SUCCESS) {
        EMSG("PTA_SECDISP_DRAW_TEXT failed, status %d!", status);
        return status;
    }

    return TEE_SUCCESS;
}

#define SECDISP_MAX_PRINT_SIZE 1024

void secdisp_printf(const char *fmt, ...)
{
    TEE_Result status;
    va_list args;
    int ccount;
    static char print_buf[SECDISP_MAX_PRINT_SIZE];

    va_start(args, fmt);
    ccount = vsnprintk(print_buf, sizeof(print_buf), fmt, args);
    va_end(args);

    if (ccount == 0) {
        return;
    }

    status = secdisp_write_text(-1, -1, print_buf, ccount);
    if (status != TEE_SUCCESS) {
        TEE_Panic(status);
    }
}
