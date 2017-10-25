/*
 * Copyright (C) Microsoft. All rights reserved
 */

#include <kernel/pseudo_ta.h>
#include <drivers/imx_spi.h>
#include <pta_spi.h>

/* Interface to the SPI driver */
struct mxc_spi_data spi_driver_data;
bool spi_driver_data_initialized = false;

/*
 * Initialize the SPI driver using the parameters specified by the
 * caller TA.
 */
static TEE_Result pta_spi_init(
    uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS]
    )
{
    enum MXC_SPI_MODE mxc_mode;
    enum spi_result spiResult;
    uint32_t expected_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,     /* SPI bus index */
        TEE_PARAM_TYPE_VALUE_INPUT,     /* SPI channel */
        TEE_PARAM_TYPE_VALUE_INPUT,     /* SPI mode */
        TEE_PARAM_TYPE_VALUE_INPUT);    /* Maximum speed, in Hz */

    FMSG("Starting");

    /* Validate the parameter types specified by the caller TA */
    if (expected_param_types != param_types) {
        EMSG("Incorrect parameter types %#x", param_types);
        return TEE_ERROR_BAD_PARAMETERS;
    }

    /* Translate PTA SPI mode into driver SPI mode value */
    switch(params[2].value.a) {
        case PTA_SPI_MODE_0:
            mxc_mode = MXC_SPI_MODE_0;
            break;
        case PTA_SPI_MODE_1:
            mxc_mode = MXC_SPI_MODE_1;
            break;
        case PTA_SPI_MODE_2:
            mxc_mode = MXC_SPI_MODE_2;
            break;
        case PTA_SPI_MODE_3:
            mxc_mode = MXC_SPI_MODE_3;
            break;
        default:
            EMSG("Incorrect PTA_SPI_MODE %#x", params[2].value.a);
            return TEE_ERROR_BAD_PARAMETERS;
    }

    if (spi_driver_data_initialized) {
        if ((params[0].value.a != spi_driver_data.bus) ||
            (params[1].value.a != spi_driver_data.channel) ||
            (mxc_mode          != spi_driver_data.mode) ||
            (params[3].value.a != spi_driver_data.maxHz)) {

            EMSG("Already initialized, with different parameters");
            return TEE_ERROR_BAD_PARAMETERS;
        }

        /* Already initialized */
        return TEE_SUCCESS;
    }

    /* Perform the initialization */
    spiResult = mxc_spi_init(
        &spi_driver_data,
        params[0].value.a,
        params[1].value.a,
        mxc_mode,
        params[3].value.a);

    if (spiResult == SPI_OK) {
        spi_driver_data.chip.ops->configure(&spi_driver_data.chip);
        spi_driver_data_initialized = true;
        return TEE_SUCCESS;
    } else {
        return TEE_ERROR_BAD_STATE;
    }
}

/* Transfer over SPI a number of bytes - both outbout and inbound */
static TEE_Result pta_spi_transfer_data(
    uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS]
    )
{
    uint32_t flags;
    uint32_t transferSize;
    uint32_t expected_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INPUT,    /* Data to send */
        TEE_PARAM_TYPE_MEMREF_OUTPUT,   /* Data to receive */
        TEE_PARAM_TYPE_VALUE_INPUT,     /* Flags */
        TEE_PARAM_TYPE_NONE);

    if (expected_param_types != param_types) {
        EMSG("Incorrect parameter types");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    transferSize = params[0].memref.size;
    
    if (transferSize != params[1].memref.size) {
        EMSG("Input size %#x is different than output size %#x",
             transferSize, params[1].memref.size);
        return TEE_ERROR_BAD_PARAMETERS;
    }

    flags = params[2].value.a;

    if (flags & (~PTA_SPI_TRANSFER_ALL_FLAGS)) {
        EMSG("Incorrect flags %#x", flags);
        return TEE_ERROR_BAD_PARAMETERS;
    }

    if (!spi_driver_data_initialized) {
        EMSG("Not initialized yet");
        return TEE_ERROR_BAD_STATE;
    }

    FMSG("Transfer size = %#x, flags = %#x", transferSize, flags);

    if (flags & PTA_SPI_TRANSFER_FLAG_START) {
        spi_driver_data.chip.ops->start(&spi_driver_data.chip);
    }

    spi_driver_data.chip.ops->txrx8(
        &spi_driver_data.chip,
        params[0].memref.buffer,
        params[1].memref.buffer,
        transferSize);

    if (flags & PTA_SPI_TRANSFER_FLAG_END) {
        spi_driver_data.chip.ops->end(&spi_driver_data.chip);
    }

    return TEE_SUCCESS;
}

/* PTA command handler */
static TEE_Result pta_spi_invoke_command(
    void *sess_ctx __unused,
    uint32_t cmd_id,
    uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS]
    )
{
    TEE_Result res;

    switch (cmd_id) {
    case PTA_SPI_COMMAND_INITIALIZE:
        res = pta_spi_init(param_types, params);
        break;

    case PTA_SPI_COMMAND_TRANSFER_DATA:
        res = pta_spi_transfer_data(param_types, params);
        break;

    default:
        EMSG("Command %#x is not supported\n", cmd_id);
        res = TEE_ERROR_NOT_IMPLEMENTED;
        break;
    }

    return res;
}

static TEE_Result pta_spi_open_session(
    uint32_t param_types __unused,
    TEE_Param params[TEE_NUM_PARAMS] __unused,
    void **sess_ctx __unused
    )
{
    /* Just TAs are allowed to use this PTA */
    if (tee_ta_get_calling_session() == NULL) {
        EMSG("Rejecting session opened from rich OS");
        return TEE_ERROR_ACCESS_DENIED;
    }

    DMSG("SPI PTA open session succeeded");
    return TEE_SUCCESS;
}

static void pta_spi_close_session(void *sess_ctx __unused)
{
    DMSG("SPI PTA close session succeeded");
}

/* The TA manager uses a mutex to synchronize calls to any of these routines */
pseudo_ta_register(.uuid = PTA_SPI_UUID, .name = "SPI_PTA",
		   .flags = PTA_DEFAULT_FLAGS,
		   .open_session_entry_point = pta_spi_open_session,
           .close_session_entry_point = pta_spi_close_session,
		   .invoke_command_entry_point = pta_spi_invoke_command);
