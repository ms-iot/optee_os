/*
 * Copyright (C) Microsoft. All rights reserved
 */

#include <kernel/spinlock.h>
#include <kernel/pseudo_ta.h>
#include <kernel/user_ta.h>
#include <drivers/imx_gpio.h>
#include <drivers/imx_iomux.h>
#include <imx-regs.h>
#include <assert.h>
#include <string.h>
#include <io.h>
#include <gpio.h>
#include <pta_spi.h>

#include <pta_ili9340.h>

#include "pta_utils.h"

#define UART_DEBUG 0
#define ILI9340_DEBUG 1

/*
 * Configuration
 */
 
#define ILI9340_CFG_SPI_BUS_INDEX 1         /* SPI2 */
#define ILI9340_CFG_SPI_CHANNEL 0           /* Channel 0 */
#define ILI9340_CFG_SPI_MODE PTA_SPI_MODE_0 /* MODE0 */
#define ILI9340_CFG_SPI_SPEED_HZ 1000000    /* SPI speed 4Mhz */

/*
 * Due to hard-wired 4 wire 8-bit interface, DCX is
 * using GPIO3_IO13.
 */
#define GPIO3_IO13  (2 * IMX_GPIO_REGISTER_BITS + 13)

/*
 * Discard buffer size
 */
#define	ILI9340_DISCARD_BUFFER_SIZE (32 * 1024)


/*
 * DCX values
 */
enum ILI9340_DCX_CODES {
    DCX_COMMAND = GPIO_LEVEL_LOW,
    DCX_DATA = GPIO_LEVEL_HIGH
};


/*
 * Auxiliary macros  
 */

#define UNREFERENCED_PARAMETER(p) (p) = (p)

#define FIELD_OFFSET(type, field) ((uint32_t)&(((type *)0)->field))
#define FIELD_SIZE(type, field) (sizeof(((type *)0)->field))

#if ILI9340_DEBUG
    #define _DMSG EMSG
#else
    #define _DMSG DMSG
#endif /* ILI9340_DEBUG */


/*
 * SPI pta interface
 */

TEE_Result pta_spi_open_session(
    uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS],
    void **sess_ctx
    );

TEE_Result pta_spi_invoke_command(
    void *sess_ctx,
    uint32_t cmd_id,
    uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS]
    );

void pta_spi_close_session(void *sess_ctx);

/*
 * ILI9340 interface
 */

static TEE_Result ili9340_open(void);
static void ili9340_close(void);


/*
 * ILI9340 driver globals
 */
static struct gpio_ops gpio;
uint32_t dcx_gpio = GPIO3_IO13;


/*
 * SPI PTA interface
 */

static TEE_Result ili9340_xchg_byte(uint32_t tx, uint32_t* rx, bool is_data)
{
    TEE_Result status;
    uint32_t discard;
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;

    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_MEMREF_OUTPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE);

    memset(params, 0, sizeof(params));
    params[0].memref.buffer = &tx;
    params[0].memref.size = sizeof(uint8_t);
    params[1].memref.buffer = rx == NULL ? &discard : rx;
    params[1].memref.size = sizeof(uint8_t);
    params[2].value.a = PTA_SPI_TRANSFER_FLAG_START|PTA_SPI_TRANSFER_FLAG_END;

    if (is_data) {
        gpio.set_value(dcx_gpio, DCX_DATA);
    } else {
        gpio.set_value(dcx_gpio, DCX_COMMAND);
    }

    status = pta_spi_invoke_command(
        NULL,
        PTA_SPI_COMMAND_TRANSFER_DATA,
        param_types,
        params);

    if (status != TEE_SUCCESS) {
        EMSG("SPI send command failed, status %d!", status);
        return status;
    }

    return TEE_SUCCESS;
}

static TEE_Result ili9340_send_cmd(uint32_t ili9340_cmd, bool is_data)
{
    TEE_Result status;
    uint32_t discard;
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;
     
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_MEMREF_OUTPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE);
        
    memset(params, 0, sizeof(params));
    params[0].memref.buffer = &ili9340_cmd;
    params[0].memref.size = sizeof(uint8_t);
    params[1].memref.buffer = &discard;
    params[1].memref.size = sizeof(uint8_t);
    params[2].value.a = PTA_SPI_TRANSFER_FLAG_START | 
        (is_data ? 0 : PTA_SPI_TRANSFER_FLAG_END);

    gpio.set_value(dcx_gpio, DCX_COMMAND);
        
    status = pta_spi_invoke_command(
        NULL,
        PTA_SPI_COMMAND_TRANSFER_DATA,
        param_types,
        params);
 
    if (status != TEE_SUCCESS) {
        EMSG("SPI send command failed, status %d!", status);
        return status;
    }
   
    return TEE_SUCCESS;
}

static TEE_Result ili9340_xchg_data(uint8_t* tx, 
        uint8_t* rx, 
        uint32_t size, 
        bool is_command)
{
    TEE_Result status;
    static uint8_t discard[ILI9340_DISCARD_BUFFER_SIZE];
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;

    assert(sizeof(discard) >= size);

    if ((tx == NULL) && (rx == NULL)) {
        return TEE_ERROR_BAD_PARAMETERS;
    } else if (tx == NULL) {
        memset(discard, 0, size);
        tx = &discard[0];
    } else if (rx == NULL) {
        rx = &discard[0];
    }

    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_MEMREF_OUTPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE);
        
    memset(params, 0, sizeof(params));
    params[0].memref.buffer = tx;
    params[0].memref.size = size;
    params[1].memref.buffer = rx;
    params[1].memref.size = size;
    params[2].value.a = PTA_SPI_TRANSFER_FLAG_END | 
        (is_command ? 0 : PTA_SPI_TRANSFER_FLAG_START);

    gpio.set_value(dcx_gpio, DCX_DATA);
        
    status = pta_spi_invoke_command(
        NULL,
        PTA_SPI_COMMAND_TRANSFER_DATA,
        param_types,
        params);
 
    if (status != TEE_SUCCESS) {
        EMSG("send data failed, status %d!", status);
        return status;
    }
   
    return TEE_SUCCESS;
}
   
/*
 * ILI9340 interface implementation
 */

static TEE_Result ili9340_open(void)
{
    TEE_Result status;
    TEE_Param params[TEE_NUM_PARAMS];
    uint32_t param_types;

    memset(params, 0, sizeof(params));
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_INPUT);
    
    memset(params, 0, sizeof(params));
    params[0].value.a = ILI9340_CFG_SPI_BUS_INDEX;
    params[1].value.a = ILI9340_CFG_SPI_CHANNEL;
    params[2].value.a = ILI9340_CFG_SPI_MODE;
    params[3].value.a = ILI9340_CFG_SPI_SPEED_HZ;
    
    status = pta_spi_invoke_command(
        NULL,
        PTA_SPI_COMMAND_INITIALIZE,
        param_types,
        params);
 
    if (status != TEE_SUCCESS) {
        EMSG("PTA_SPI_COMMAND_INITIALIZE failed, status %d!", status);
        ili9340_close();
        return status;
    }
    
    mxc_gpio_init(&gpio);
    gpio.set_direction(dcx_gpio, GPIO_DIR_OUT);
    
    return TEE_SUCCESS;
}

static void ili9340_close(void)
{
}

/*
 * Command handlers
 */

static TEE_Result ili9340_cmd_xchg_single(uint32_t param_types,
                    TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result status;
    bool is_data;

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_OUTPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    if (exp_param_types != param_types) {
        EMSG("Incorrect parameter types");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    is_data = params[0].value.b != 0;

    status = ili9340_xchg_byte(params[0].value.a, &params[1].value.a, is_data);

    _DMSG(
        "ili9340_cmd_xchg_single: data 0x%x, cmd(0)/data(1) %d, rx 0x%X",
        params[0].value.a,
        params[0].value.b,
        params[1].value.a);

    if (status != TEE_SUCCESS) {
        EMSG("ili9340_xchg_byte failed, status 0x%X", status);
        return status;
    }

    return TEE_SUCCESS;
}

static TEE_Result ili9340_cmd_xchg(uint32_t param_types,
                    TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result status;
    bool is_command = false;
    bool is_data = false;
    uint8_t* tx_data = NULL;
    uint8_t* rx_data = NULL;
    uint32_t data_size = 0;

    if (TEE_PARAM_TYPE_GET(param_types, 0) != TEE_PARAM_TYPE_VALUE_INPUT) {
        return TEE_ERROR_BAD_PARAMETERS;
    }

    is_command = params[0].value.b != 0;
    data_size = params[1].memref.size;

    if (data_size != 0) {
        tx_data = (uint8_t*)params[1].memref.buffer;
        is_data = true;
    }

    if (params[2].memref.size != 0) {
        if ((data_size != 0) && (data_size != params[2].memref.size)) {
            EMSG("ili9340_cmd_xchg failed, rx/tx buffers size dont match");
            return TEE_ERROR_BAD_PARAMETERS;
        }
        data_size = params[2].memref.size;
        rx_data = (uint8_t*)params[2].memref.buffer;
        is_data = true;
    }
       
    _DMSG(
        "ili9340_cmd_xchg: cmd 0x%x, valid %d, data size %d",
        params[0].value.a,
        params[0].value.b,
        data_size);
 
    if (is_command) {
        status = ili9340_send_cmd(params[0].value.a, is_data);
        if (status != TEE_SUCCESS) {
            EMSG("ili9340_send_cmd failed, status 0x%X", status);
            return status;
        }
    }

    if (is_data) {
        status = ili9340_xchg_data(tx_data, rx_data, data_size, is_command);

        if (status != TEE_SUCCESS) {
            EMSG("ili9340_send_data failed, status 0x%X", status);
            return status;
        }
    }
    
    return TEE_SUCCESS;
}

/* 
 * Trusted Application Entry Points
 */

static TEE_Result pta_ili9340_open_session(uint32_t param_types __unused,
                    TEE_Param params[TEE_NUM_PARAMS] __unused,
                    void **sess_ctx __unused)
{
    TEE_Result status;
    struct tee_ta_session *session;
    
    session = tee_ta_get_calling_session();
    
    /* Access is restricted to TA only */
    if (session == NULL) {
        EMSG("ili9340 open session failed, REE access is not allowed!");
        return TEE_ERROR_ACCESS_DENIED;
    }
    
    status = ili9340_open();
    if (status != TEE_SUCCESS) {
        EMSG("ili9340_open failed, status 0x%X", status);
        return status;
    }        
    
    _DMSG("ili9340 open session succeeded!");
    return TEE_SUCCESS;
}

static void pta_ili9340_close_session(void *sess_ctx __unused)
{
    ili9340_close();
    
    _DMSG("ili9340 close session succeeded!");
}

static TEE_Result pta_ili9340_invoke_command(void *sess_ctx __unused, uint32_t cmd_id,
				 uint32_t param_types,
				 TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result res;
    
    _DMSG("ili9340 invoke command %d\n", cmd_id);

    switch (cmd_id) {
    case PTA_ILI9340_INIT:
        res = TEE_SUCCESS;
        break;

    case PTA_ILI9340_XCHG_SINGLE:
        res = ili9340_cmd_xchg_single(param_types, params);
        break;

    case PTA_ILI9340_XCHG:
        res = ili9340_cmd_xchg(param_types, params);
        break;

    default:
        EMSG("Command not implemented %d", cmd_id);
        res = TEE_ERROR_NOT_IMPLEMENTED;
        break;
    }

    return res;
}

pseudo_ta_register(.uuid = PTA_ILI9340_UUID, .name = "pta_ili9340",
    .flags = PTA_DEFAULT_FLAGS,
    .open_session_entry_point = pta_ili9340_open_session,
    .close_session_entry_point = pta_ili9340_close_session,
    .invoke_command_entry_point = pta_ili9340_invoke_command);
