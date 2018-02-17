/*
 * Copyright (C) Microsoft. All rights reserved
 */

#include <kernel/pseudo_ta.h>
#include <kernel/user_ta.h>
#include <assert.h>
#include <string.h>
#include <drivers/secdisp_drv.h>

#include <pta_secdisp.h>

#include "pta_utils.h"

#define SECDISP_DEBUG 1

#if SECDISP_DEBUG
    #define _DMSG EMSG
#else
    #define _DMSG DMSG
#endif /* SECDISP_DEBUG */

/*
 * Supported drivers interface
 */
TEE_Result ili9340_drv_init(struct secdisp_driver* driver);

 /*
  * Driver interface
  */
static struct secdisp_driver drv = { -1 };


/*
 * SECDISP PTA globals
 */
  
/*
 * Command handlers
 */

static TEE_Result secdisp_cmd_init(
    uint32_t param_types, 
    TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result status;
    uint32_t drv_id;

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_MEMREF_OUTPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    if ((exp_param_types != param_types) ||
        (params[1].memref.size < sizeof(SECDISP_INFORMATION))) {
        EMSG("Incorrect parameter types");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    drv_id = params[0].value.a;

    switch (drv_id) {
    case SECDIP_DRIVER_ILI9340:
        status = ili9340_drv_init(&drv);
        break;

    default:
        EMSG("Unsupported driver id %d", drv_id);
        status = TEE_ERROR_NOT_SUPPORTED;
    }

    if (status == TEE_SUCCESS) {
        memcpy(params[1].memref.buffer, &drv.disp_info, sizeof(SECDISP_INFORMATION));
    }

    return status;
}

static TEE_Result secdisp_cmd_draw_pixel(
    uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result status;

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    if (exp_param_types != param_types) {
        EMSG("Incorrect parameter types");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    status = drv.ops->draw_pixel(&drv, 
        params[0].value.a,  /* x */
        params[0].value.b,  /* y */
        params[1].value.a); /* color */

    return status;
}

static TEE_Result secdisp_cmd_draw_line(
    uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS],
    bool is_vertical)
{
    TEE_Result status;

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE);

    if (exp_param_types != param_types) {
        EMSG("Incorrect parameter types");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    status = drv.ops->draw_line(&drv,
        params[0].value.a, /* x */
        params[0].value.b, /* y */
        params[1].value.a, /* size */
        params[2].value.a, /* color */
        is_vertical); 

    return status;
}

static TEE_Result secdisp_cmd_fill_rect(
    uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result status;

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_OUTPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    if (exp_param_types != param_types) {
        EMSG("Incorrect parameter types");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    status = drv.ops->fill_rect(&drv,
        params[0].value.a,  /* x */
        params[0].value.b,  /* y */
        params[1].value.a,  /* width */
        params[1].value.b,  /* height */
        params[2].value.a); /* color */

    return status;
}

static TEE_Result secdisp_cmd_set_rotation(
    uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result status;

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    if (exp_param_types != param_types) {
        EMSG("Incorrect parameter types");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    status = drv.ops->set_rotation(&drv,
        params[0].value.a); /* rotation */

    return status;
}

static TEE_Result secdisp_cmd_invert_display(
    uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result status;

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    if (exp_param_types != param_types) {
        EMSG("Incorrect parameter types");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    status = drv.ops->invert_display(&drv,
        params[0].value.a != 0); /* is invert */

    return status;
}

static TEE_Result secdisp_cmd_set_text_pos(
    uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result status;

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    if (exp_param_types != param_types) {
        EMSG("Incorrect parameter types");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    status = drv.ops->set_text_pos(&drv,
        params[0].value.a,  /* x */
        params[0].value.b); /* y */

    return status;
}

static TEE_Result secdisp_cmd_draw_text(
    uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result status;

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_NONE);

    if (exp_param_types != param_types) {
        EMSG("Incorrect parameter types");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    status = drv.ops->draw_text(&drv,
        params[0].value.a,      /* x */
        params[0].value.b,      /* y */
        params[1].value.a,      /* color */
        params[2].memref.buffer,/* text */
        params[0].memref.size); /* count */

    return status;
}

/* 
 * Trusted Application Entry Points
 */

static TEE_Result pta_secdisp_open_session(uint32_t param_types __unused,
                    TEE_Param params[TEE_NUM_PARAMS] __unused,
                    void **sess_ctx __unused)
{
    //TEE_Result status;
    struct tee_ta_session *session;
    
    session = tee_ta_get_calling_session();
    
    /* Access is restricted to TA only */
    if (session == NULL) {
        EMSG("secdisp open session failed, REE access is not allowed!");
        return TEE_ERROR_ACCESS_DENIED;
    }
    
    _DMSG("secdisp open session succeeded!");
    return TEE_SUCCESS;
}

static void pta_secdisp_close_session(void *sess_ctx __unused)
{
    _DMSG("secdisp close session succeeded!");
}

static TEE_Result pta_secdisp_invoke_command(
                    void *sess_ctx __unused, 
                    uint32_t cmd_id,
                    uint32_t param_types,
                    TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result res;
    
    _DMSG("secdisp invoke command %d\n", cmd_id);

    switch (cmd_id) {
    case PTA_SECDISP_INIT:
        res = secdisp_cmd_init(param_types, params);
        break;

    case PTA_SECDISP_DRAW_PIXEL:
        res = secdisp_cmd_draw_pixel(param_types, params);
        break;

    case PTA_SECDISP_DRAW_VLINE:
    case PTA_SECDISP_DRAW_HLINE:
        res = secdisp_cmd_draw_line(param_types, params, 
                cmd_id == PTA_SECDISP_DRAW_VLINE);
        
        break;

    case PTA_SECDISP_FILL_RECT:
        res = secdisp_cmd_fill_rect(param_types, params);
        break;

    case PTA_SECDISP_SET_ROTATION:
        res = secdisp_cmd_set_rotation(param_types, params);
        break;

    case PTA_SECDISP_INVERT_DISPLAY:
        res = secdisp_cmd_invert_display(param_types, params);
        break;

    case PTA_SECDISP_SET_TEXT_POS:
        res = secdisp_cmd_set_text_pos(param_types, params);
        break;

    case PTA_SECDISP_DRAW_TEXT:
        res = secdisp_cmd_draw_text(param_types, params);
        break;

    default:
        EMSG("command not implemented %d", cmd_id);
        res = TEE_ERROR_NOT_IMPLEMENTED;
        break;
    }

    return res;
}

pseudo_ta_register(.uuid = PTA_SECDISP_UUID, .name = "pta_secdisp",
    .flags = PTA_DEFAULT_FLAGS,
    .open_session_entry_point = pta_secdisp_open_session,
    .close_session_entry_point = pta_secdisp_close_session,
    .invoke_command_entry_point = pta_secdisp_invoke_command);
