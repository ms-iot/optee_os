/*
 * Copyright (C) Microsoft. All rights reserved
 */

#include <arm.h>
#include <kernel/misc.h>
#include <kernel/spinlock.h>
#include <kernel/pseudo_ta.h>
#include <kernel/user_ta.h>
#include <kernel/tee_time.h>
#include <kernel/thread.h>
#include <mm/core_memprot.h>
#include <mm/tee_mmu.h>
#include <optee_msg_supplicant.h>
#include <utee_defines.h>
#include <string.h>
#include <drivers/imx_uart.h>

#include <pta_gt511c3.h>

#include "pta_utils.h"

#define UART_DEBUG 0
#define GT511C3_DEBUG 0

/*
 * Baud rate after reset
 */
#define GT511C3_RESET_BAUDRATE 9600

/*
 * Max baud rate
 */
#define GT511C3_MAX_BAUDRATE 115200

/*
 * Largest frame that can be read from scanner
 */
#define GT511C3_MAX_FRAME (64 * 1024)

/*
 * Scanner command response timeout in mSec
 */
#define GT511C3_RESPONSE_TIMEOUT_MSEC 2000L

/*
 * Max RX retry
 */
#define RX_RETRY_COUNT 10000L

/*
 * NO RX data code
 */
#define NO_RX_DATA ((uint16_t)-1)

/* 
 * For timeout calculations assume 10 bits/char
 */
#define	BITS_PER_CHAR 10L

/*
 * Minimum RX timeout in mSec
 */
#define MIN_RX_TIMEOUT_MSEC 50L


/*
 * GT511C3 error codes
 */

enum GT511C3_STATUS {
    GT511C3_SUCCESS = 0x0000,	                    // Success
    GT511C3_STATUS_TIMEOUT = 0x1001,	            // Obsolete, capture timeout 
    GT511C3_STATUS_INVALID_BAUDRATE = 0x1002,	    // Obsolete, Invalid serial baud rate 
    GT511C3_STATUS_INVALID_POS = 0x1003,	    // The specified ID is not between 0~199 
    GT511C3_STATUS_IS_NOT_USED = 0x1004,	    // The specified ID is not used 
    GT511C3_STATUS_IS_ALREADY_USED = 0x1005,	    // The specified ID is already used 
    GT511C3_STATUS_COMM_ERR = 0x1006,	            // Communication Error 
    GT511C3_STATUS_VERIFY_FAILED = 0x1007,	    // 1:1 Verification Failure 
    GT511C3_STATUS_IDENTIFY_FAILED = 0x1008,	    // 1:N Identification Failure 
    GT511C3_STATUS_DB_IS_FULL = 0x1009,	            // The database is full 
    GT511C3_STATUS_DB_IS_EMPTY = 0x100A,	    // The database is empty 
    GT511C3_STATUS_TURN_ERR = 0x100B,	            // Obsolete, Invalid order of the enrollment (The order was not as: EnrollStart -> Enroll1 -> Enroll2 -> Enroll3) 
    GT511C3_STATUS_BAD_FINGER = 0x100C,	            // Too bad fingerprint 
    GT511C3_STATUS_ENROLL_FAILED = 0x100D,	    // Enrollment Failure 
    GT511C3_STATUS_IS_NOT_SUPPORTED = 0x100E,	    // The specified command is not supported 
    GT511C3_STATUS_DEV_ERR = 0x100F,	            // Device Error, especially if Crypto-Chip is trouble 
    GT511C3_STATUS_CAPTURE_CANCELED = 0x1010,	    // Obsolete, The capturing is canceled 
    GT511C3_STATUS_INVALID_PARAM = 0x1011,	    // Invalid parameter 
    GT511C3_STATUS_FINGER_IS_NOT_PRESSED = 0x1012,  // Finger is not pressed 
    GT511C3_STATUS_INVALID = 0XFFFF	            // Used when parsing fails 
};


/*
 * GT511C3 command codes
 */

enum GT511C3_COMMANDS {
    GT511C3_CMD_INVALID = 0x00,
    GT511C3_CMD_OPEN = 0x01,                // Open Initialization 
    GT511C3_CMD_CLOSE = 0x02,               // Close Termination 
    GT511C3_CMD_USB_INTERNAL_CHECK = 0x03,  // UsbInternalCheck Check if the connected USB device is valid 
    GT511C3_CMD_CHANGE_BAUD_RATE = 0x04,    // ChangeBaudrate Change UART baud rate 
    GT511C3_CMD_SET_IAP_MODE = 0x05,        // SetIAPMode Enter IAP Mode In this mode, FW Upgrade is available 
    GT511C3_CMD_CMOS_LED = 0x12,            // CmosLed Control CMOS LED 
    GT511C3_CMD_GET_ENROLL_COUNT = 0x20,    // Get enrolled fingerprint count 
    GT511C3_CMD_CHECK_ENROLLED = 0x21,	    // Check whether the specified ID is already enrolled 
    GT511C3_CMD_ENROL_START = 0x22,	    // Start an enrollment 
    GT511C3_CMD_ENROLL1 = 0x23,		    // Make 1st template for an enrollment 
    GT511C3_CMD_ENROLL2 = 0x24,		    // Make 2nd template for an enrollment 
    GT511C3_CMD_ENROLL3 = 0x25,		    // Make 3rd template for an enrollment, merge three templates into one template, save merged template to the database 
    GT511C3_CMD_IS_PRESS_FINGER = 0x26,	    // Check if a finger is placed on the sensor 
    GT511C3_CMD_DELETE_ID = 0x40,	    // Delete the fingerprint with the specified ID 
    GT511C3_CMD_DELETE_ALL = 0x41,	    // Delete all fingerprints from the database 
    GT511C3_CMD_VERIFY1_1 = 0x50,	    // Verification of the capture fingerprint image with the specified ID 
    GT511C3_CMD_IDENTIFY1_N = 0x51,	    // Identification of the capture fingerprint image with the database 
    GT511C3_CMD_VERIFY_TEMPLATE1_1 = 0x52,  // Verification of a fingerprint template with the specified ID 
    GT511C3_CMD_IDENTIFY_TEMPLATE1_N = 0x53,// Identification of a fingerprint template with the database 
    GT511C3_CMD_CAPTURE_FINGER = 0x60,	    // Capture a fingerprint image(256x256) from the sensor 
    GT511C3_CMD_MAKE_TEMPLATE = 0x61,	    // Make template for transmission 
    GT511C3_CMD_GET_IMAGE = 0x62,	    // Download the captured fingerprint image(256x256) 
    GT511C3_CMD_GET_RAW_IMAGE = 0x63,	    // Capture & Download raw fingerprint image(320x240) 
    GT511C3_CMD_GET_TEMPLATE = 0x70,        // Download the template of the specified ID 
    GT511C3_CMD_SET_TEMPLATE = 0x71,	    // Upload the template of the specified ID 
    GT511C3_CMD_GET_DATABASE_START = 0x72,  // Start database download, obsolete 
    GT511C3_CMD_GET_DATABASE_END = 0x73,    // End database download, obsolete 
    GT511C3_CMD_UPGRADE_FIRMWARE = 0x80,    // Not supported 
    GT511C3_CMD_UPGRADE_ISOCD_IMAGE = 0x81, // Not supported 
    GT511C3_CMD_ACK = 0x30,		    // Acknowledge. 
    GT511C3_CMD_NACK = 0x31		    // Non-acknowledge 
};


/*
 * Communication protocol related definitions
 */

/*
 * Command frame
 */

enum GT511C3_COMMAND_FRAME_CODES {
    GT511C3_CMD_START_CODE1 = 0x55,
    GT511C3_CMD_START_CODE2 = 0xAA,

    GT511C3_CMD_DEVICE_ID = 0x0001
};

struct _gt511c3_command {
    uint8_t start_code1;    // #0 : GT511C3_CMD_START_CODE1
    uint8_t start_code2;    // #1 : GT511C3_CMD_START_CODE2
    int16_t device_id;      // #2 : Device ID: always 0x0001
    uint32_t parameter;     // #4 : Input parameter
    int16_t command;        // #8 : Command code: gt511c3_commands
    int16_t checksum;       // #10: Checksum (byte addition)
} __packed;
typedef struct _gt511c3_command gt511c3_command;


/*
 * Response frame
 */

enum GT511C3_RESPONSE_FRAME_CODES {
    GT511C3_RSP_START_CODE1 = 0x55,
    GT511C3_RSP_START_CODE2 = 0xAA,

    GT511C3_RSP_DEVICE_ID = 0x0001,

    GT511C3_RSP_ACK = 0x30,
    GT511C3_RSP_NACK = 0x31
};

struct _gt511c3_response {
    uint8_t start_code1;    // #0 : GT511C3_RSP_START_CODE1
    uint8_t start_code2;    // #1 : GT511C3_RSP_START_CODE2
    int16_t device_id;      // #2 : Device ID: always 0x0001
    uint32_t parameter;     // #4 : If response is GT511C3_RSP_ACK -> Output parameter
                            //      If response is GT511C3_RSP_NACK -> Error code
    int16_t response;       // #8 : Response: GT511C3_RSP_ACK/GT511C3_RSP_ACK
    int16_t checksum;       // #10: Checksum (byte addition)
} __packed;
typedef struct _gt511c3_response gt511c3_response;


/*
 * Data frame
 */

enum GT511C3_DATA_FRAME_CODES {
    GT511C3_DATA_START_CODE1 = 0x5A,
    GT511C3_DATA_START_CODE2 = 0xA5,

    GT511C3_DATA_DEVICE_ID = 0x0001,
};

struct _gt511c3_data {
    uint8_t start_code1;    // #0 : GT511C3_DATA_START_CODE1
    uint8_t start_code2;    // #1 : GT511C3_DATA_START_CODE2
    int16_t device_id;      // #2 : Device ID: always 0x0001
    uint8_t data[1];        // #4 : Data bytes N
                            // ...
                            //int16_t checksum;     // #4 + N: Checksum (byte addition)
} __packed;
typedef struct _gt511c3_data gt511c3_data;



/*
 * Auxiliary macros
 */

#define GT511C3_IS_VALID_CONFIG(config) ((config)->uart_base_pa != 0)
#define GT511C3_INVALIDATE_CONFIG(config) \
    (memset((config), 0, sizeof(GT511C3_DeviceConfig)))

#define GT511C3_INIT_COMMAND_FRAME(cmd_frame, cmd_code) { \
    memset((cmd_frame),0,sizeof(gt511c3_command)); \
    (cmd_frame)->start_code1 = GT511C3_CMD_START_CODE1; \
    (cmd_frame)->start_code2 = GT511C3_CMD_START_CODE2; \
    (cmd_frame)->device_id = GT511C3_CMD_DEVICE_ID; \
    (cmd_frame)->command = (cmd_code); \
}

#define GT511C3_INIT_DATA_FRAME(data_frame, payload_size) { \
    memset((data_frame),0,GT511C3_DATA_FRAME_SIZE(payload_size)); \
    (data_frame)->start_code1 = GT511C3_DATA_START_CODE1; \
    (data_frame)->start_code2 = GT511C3_DATA_START_CODE2; \
    (data_frame)->device_id = GT511C3_DATA_DEVICE_ID; \
}

#define GT511C3_CS_SIZE (sizeof(int16_t))

#define GT511C3_DATA_FRAME_SIZE(payload_size) \
    (FIELD_OFFSET(gt511c3_data, data) + (payload_size))

#define GT511C3_DATA_HEADER_SIZE (GT511C3_DATA_FRAME_SIZE(0))

#if GT511C3_DEBUG
    #define _DMSG EMSG
#else
    #define _DMSG DMSG
#endif /* GT511C3_DEBUG */


/*
 * GT511C3 interface
 */

static TEE_Result gt511c3_open(GT511C3_DeviceConfig *device_config,
            GT511C3_DeviceInfo *device_info __maybe_unused);

static TEE_Result gt511c3_close(void);


/*
 * GT511C3 driver globals
 */

static unsigned int gt511c3_lock = SPINLOCK_UNLOCK;
static struct tee_ta_session *curr_session = NULL;
static struct imx_uart_data uart_driver;
static struct serial_chip *serial = NULL;
static bool is_com_error = false;
static GT511C3_DeviceConfig lkg_device_config = {0};
static uint32_t usec_per_char;


/*
 * GT511C3 support methods
 */

static TEE_Result gt511c3_to_tee(uint32_t status)
{
    switch (status) {
    case GT511C3_SUCCESS:
        return TEE_SUCCESS;

    case GT511C3_STATUS_TIMEOUT:
        return TEE_ERROR_COMMUNICATION;

    case GT511C3_STATUS_INVALID_BAUDRATE:
        return TEE_ERROR_BAD_PARAMETERS;

    case GT511C3_STATUS_INVALID_POS:
        return TEE_ERROR_BAD_STATE;

    case GT511C3_STATUS_IS_NOT_USED:
        return TEE_ERROR_BAD_STATE;

    case GT511C3_STATUS_IS_ALREADY_USED:
        return TEE_ERROR_BUSY;

    case GT511C3_STATUS_COMM_ERR:
        return TEE_ERROR_COMMUNICATION;

    case GT511C3_STATUS_VERIFY_FAILED:
        return TEE_ERROR_ACCESS_DENIED;

    case GT511C3_STATUS_IDENTIFY_FAILED:
        return TEE_ERROR_ACCESS_DENIED;

    case GT511C3_STATUS_DB_IS_FULL:
        return TEE_ERROR_OUT_OF_MEMORY;

    case GT511C3_STATUS_DB_IS_EMPTY:
        return TEE_ERROR_NO_DATA;

    case GT511C3_STATUS_TURN_ERR:
        return TEE_ERROR_BAD_STATE;

    case GT511C3_STATUS_BAD_FINGER:
        return TEE_ERROR_BAD_STATE;

    case GT511C3_STATUS_ENROLL_FAILED:
        return TEE_ERROR_BAD_STATE;

    case GT511C3_STATUS_IS_NOT_SUPPORTED:
        return TEE_ERROR_NOT_SUPPORTED;

    case GT511C3_STATUS_DEV_ERR:
        return TEE_ERROR_BAD_STATE;

    case GT511C3_STATUS_CAPTURE_CANCELED:
        return TEE_ERROR_CANCEL;

    case GT511C3_STATUS_INVALID_PARAM:
        return TEE_ERROR_BAD_PARAMETERS;

    case GT511C3_STATUS_FINGER_IS_NOT_PRESSED:
        return TEE_ERROR_BAD_STATE;

    case GT511C3_STATUS_INVALID:
        return TEE_ERROR_GENERIC;

    default:
        return TEE_ERROR_GENERIC;
    }
}


/*
 * UART interface
 */

static TEE_Result gt511c3_init(GT511C3_DeviceConfig *device_config,
                      bool is_reset_baudrate)
{
    struct imx_uart_config uart_config = {
        .clock_hz = device_config->uart_clock_hz,
        .baud_rate = is_reset_baudrate ?
                         GT511C3_RESET_BAUDRATE :
                         device_config->baud_rate
    };

    if (uart_config.baud_rate > GT511C3_MAX_BAUDRATE) {
        return TEE_ERROR_BAD_PARAMETERS;
    }

    if (!imx_uart_init_ex(&uart_driver,
            device_config->uart_base_pa,
            &uart_config)) {

        return TEE_ERROR_BAD_PARAMETERS;
    }

    serial = &uart_driver.chip;

    /*
     * RX timeout parameters, assuming 10 bits/char
     */
    usec_per_char = BITS_PER_CHAR * (1000000L/uart_config.baud_rate + 1);
    is_com_error = false;

    return TEE_SUCCESS;
}
 
static uint32_t gt611c3_get_frame_timeout_msec(uint32_t length)
{
    uint32_t rx_frame_timeout_msec;

    rx_frame_timeout_msec = usec_per_char * length / 1000L;
    rx_frame_timeout_msec = rx_frame_timeout_msec * 3 / 2;

    if (rx_frame_timeout_msec < MIN_RX_TIMEOUT_MSEC) {
        rx_frame_timeout_msec = MIN_RX_TIMEOUT_MSEC;
    }

    return rx_frame_timeout_msec;
}

static uint16_t gt511c3_get_byte(void) 
{
#if UART_DEBUG
    for (;;) {
#else
    uint32_t i;

    for (i = 0; i < RX_RETRY_COUNT; ++i) {
#endif
        if (serial->ops->have_rx_data(serial)) {
            return (uint16_t)serial->ops->getchar(serial);
        }
    }

    return NO_RX_DATA;
}
 
#if UART_DEBUG
static bool
uart_test(void)
{
    static const char* uart_test_in = "12345678";
    static const char* uart_out = "GT5311C3 UART Test";
    char uart_in[8];
    uint16_t b;
    uint32_t i;

    for (i = 0; i < strlen(&uart_out[0]); ++i) {
        serial->ops->putc(serial, uart_out[i]);
    }
    
    for (i = 0; i < strlen(&uart_test_in[0]); ++i) {
        b = gt511c3_get_byte();
        if (b == NO_RX_DATA) {
            return false;
        } 
        uart_in[i] = (uint8_t)b;

        if (uart_in[i] != uart_test_in[i]) {
            return false;
        }
    }

    return true;
}
#endif // UART_DEBUG
 
 
/*
 * GT511C3 protocol implementation
 */

static int16_t gt511c3_checksum(uint8_t *msg, uint16_t length)
{
    uint16_t i;
    int16_t cs;

    cs = 0;
    for (i = 0; i < length; ++i) cs += msg[i];

    return cs;
}

static TEE_Result gt511c3_recv(uint8_t *rx_data, uint32_t length)
{
    uint16_t b;
    uint32_t i;
    uint32_t rx_timeout_msec;
    stopwatch sw;

    rx_timeout_msec = GT511C3_RESPONSE_TIMEOUT_MSEC;
    stopwatch_start(&sw);

    for (i = 0; i < length; ) {
        b = gt511c3_get_byte();
        if (b == NO_RX_DATA) {
            if (stopwatch_elapsed_millis(&sw) < rx_timeout_msec) {
                continue;
            }

            EMSG("gt511c3_recv rx data timeout! Got %d out of %d", i, length);
            is_com_error = true;
            return TEE_ERROR_NO_DATA;
        }

        if (i == 0) {
            rx_timeout_msec = gt611c3_get_frame_timeout_msec(length);
            stopwatch_start(&sw);
        }
        rx_data[i++] = (uint8_t)b;
    }
    
    return TEE_SUCCESS;
}

static TEE_Result gt511c3_recv_response(gt511c3_response *rsp)
{
    TEE_Result status;

    status = gt511c3_recv((uint8_t *)rsp, sizeof(gt511c3_response));
    if (status != TEE_SUCCESS) {
        rsp->parameter = GT511C3_STATUS_TIMEOUT;
        return status;
    }

    if ((rsp->start_code1 != GT511C3_RSP_START_CODE1) ||
        (rsp->start_code2 != GT511C3_RSP_START_CODE2)) {

        EMSG("gt511c3 - response framing error");
        rsp->parameter = GT511C3_STATUS_COMM_ERR;
        is_com_error = true;
        return TEE_ERROR_COMMUNICATION;
    }

    if (rsp->checksum != gt511c3_checksum(
                            (uint8_t*)rsp, 
                            FIELD_OFFSET(gt511c3_response, checksum))) {

        EMSG("gt511c3 - response checksum error");
        rsp->parameter = GT511C3_STATUS_COMM_ERR;
        is_com_error = true;
        return TEE_ERROR_COMMUNICATION;
    }

    if (rsp->response == GT511C3_RSP_NACK) {
        return gt511c3_to_tee(rsp->parameter);
    }

    return TEE_SUCCESS;
}

static TEE_Result gt511c3_recv_data(uint8_t* data, uint32_t length)
{
    TEE_Result status;
    gt511c3_data data_frame;
    int16_t calc_cs;
    int16_t msg_cs;

    if (length > GT511C3_MAX_FRAME) {
        return TEE_ERROR_BAD_PARAMETERS;
    }

    status = gt511c3_recv((uint8_t*)&data_frame, GT511C3_DATA_FRAME_SIZE(0));
    if (status != TEE_SUCCESS) {
        return status;
    }

    status = gt511c3_recv(data, length + GT511C3_CS_SIZE);
    if (status != TEE_SUCCESS) {
        return status;
    }

    if ((data_frame.start_code1 != GT511C3_DATA_START_CODE1) ||
        (data_frame.start_code2 != GT511C3_DATA_START_CODE2)) {

        EMSG("gt511c3 - rx data framing error");
        is_com_error = true;
        return TEE_ERROR_COMMUNICATION;
    }

    calc_cs = gt511c3_checksum((uint8_t*)&data_frame, GT511C3_DATA_FRAME_SIZE(0));
    calc_cs += gt511c3_checksum(data, length); 
    msg_cs = (int16_t)((data[length + 1] << 8) + data[length]);

    if (calc_cs != msg_cs) {
        EMSG("gt511c3 - rx data checksum error");
        is_com_error = true;
        return TEE_ERROR_COMMUNICATION;
    }

    return TEE_SUCCESS;
}

static TEE_Result gt511c3_send_cmd(
    gt511c3_command *cmd,
    uint32_t *result __maybe_unused)
{
    TEE_Result status;
    uint8_t *payload = (uint8_t *)cmd;
    gt511c3_response rsp;
    uint32_t i;

    if (is_com_error) {
        EMSG("flushing UART after failure");
        serial->ops->flush(serial);
        is_com_error = false;
    }

    cmd->checksum = gt511c3_checksum(
                        (uint8_t*)cmd, 
                         FIELD_OFFSET(gt511c3_command, checksum));

    for (i = 0; i < sizeof(gt511c3_command); ++i) {
        serial->ops->putc(serial, payload[i]);
    }
 
    status = gt511c3_recv_response(&rsp);

    if (result != NULL) {
        *result = rsp.parameter;
    }

    return status;
}

static TEE_Result gt511c3_send_data(
    uint8_t *payload,
    uint32_t payload_size,
    uint32_t *result __maybe_unused)
{
    TEE_Result status;
    int16_t cs;
    uint8_t *tx_data; 
    gt511c3_data data_frame;
    gt511c3_response rsp;
    uint32_t i;
 
    if ((payload == NULL) || (payload_size == 0)) {
        return TEE_ERROR_BAD_PARAMETERS;
    }

    GT511C3_INIT_DATA_FRAME(&data_frame, 0);

    cs = gt511c3_checksum((uint8_t*)&data_frame, 
            GT511C3_DATA_FRAME_SIZE(0));

    cs += gt511c3_checksum(payload, payload_size);

    tx_data = (uint8_t*)&data_frame;
    for (i = 0; i < GT511C3_DATA_FRAME_SIZE(0); ++i) {
        serial->ops->putc(serial, tx_data[i]);
    }

    tx_data = payload;
    for (i = 0; i < payload_size; ++i) {
        serial->ops->putc(serial, tx_data[i]);
    }

    tx_data = (uint8_t*)&cs;
    for (i = 0; i < sizeof(cs); ++i) {
        serial->ops->putc(serial, tx_data[i]);
    }
 
    status = gt511c3_recv_response(&rsp);

    if (result != NULL) {
        *result = rsp.parameter;
    }

    return status;
}


/*
 * GT511C3 interface implementation
 */

static TEE_Result gt511c3_open(GT511C3_DeviceConfig *device_config,
                    GT511C3_DeviceInfo *device_info __maybe_unused)
{
    TEE_Result status;
    gt511c3_command cmd;

    if (device_config == NULL) {
        EMSG("gt511c3_open failed, missing device configuration");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    status = gt511c3_init(device_config, true);
    if (status != TEE_SUCCESS) {
        EMSG("gt511c3_init failed, status 0x%X \n", status);
        return status;
    }

#if UART_DEBUG
    UNREFERENCED_PARAMETER(cmd);

    if (!uart_test()) {
        return TEE_ERROR_BAD_STATE;
    }
#else  
    GT511C3_INIT_COMMAND_FRAME(&cmd, GT511C3_CMD_OPEN);
    if (device_info != NULL) {
        cmd.parameter = 1;
    }

    status = gt511c3_send_cmd(&cmd, NULL);
    if (status != TEE_SUCCESS) {
        GT511C3_DeviceConfig alt_device_config;

        memcpy(&alt_device_config, device_config, sizeof(*device_config));
        alt_device_config.baud_rate = GT511C3_MAX_BAUDRATE;

        status = gt511c3_init(&alt_device_config, false);
        if (status != TEE_SUCCESS) {
            EMSG("gt511c3_init failed, status 0x%X \n", status);
            return status;
        }

        EMSG("initial gt511c3_send_cmd failed, first chance retrying...");

        status = gt511c3_send_cmd(&cmd, NULL);
        if (status != TEE_SUCCESS) {
            EMSG("initial gt511c3_send_cmd failed, aborting");
            return status;
        }
    }

    if (device_info != NULL) {
        status = gt511c3_recv_data((uint8_t*)device_info, sizeof(*device_info));
        if (status != TEE_SUCCESS) {
            EMSG("gt511c3_recv_data failed, status 0x%X", status);
            return status;
        }
    }

    if (device_config->baud_rate != GT511C3_RESET_BAUDRATE) {
        GT511C3_INIT_COMMAND_FRAME(&cmd, GT511C3_CMD_CHANGE_BAUD_RATE);
        cmd.parameter = device_config->baud_rate;

        status = gt511c3_send_cmd(&cmd, NULL);
        if (status != TEE_SUCCESS) {
            EMSG("set baudrate failed, status 0x%X", status);
            return status;
        }

        status = gt511c3_init(device_config, false);
        if (status != TEE_SUCCESS) {
            EMSG("gt511c3_init failed, status 0x%X", status);
            return status;
        }
    }
#endif

    memcpy(&lkg_device_config, device_config, sizeof(*device_config));
    return TEE_SUCCESS;
}

static TEE_Result gt511c3_close(void)
{
#if UART_DEBUG
    return TEE_SUCCESS;
#else
    gt511c3_command cmd;
    TEE_Result status;

    status = TEE_SUCCESS;

    /*
     * If the device was opened successfully, we set it back
     * to a known state, with the reset baud rate.
     */

    if (GT511C3_IS_VALID_CONFIG(&lkg_device_config)) {
        GT511C3_INIT_COMMAND_FRAME(&cmd, GT511C3_CMD_CHANGE_BAUD_RATE);
        cmd.parameter = GT511C3_RESET_BAUDRATE;

        status = gt511c3_send_cmd(&cmd, NULL);
        if (status != TEE_SUCCESS) {
            EMSG("set baudrate failed, status 0x%X", status);
        }

        (void)gt511c3_init(&lkg_device_config, true);
    }

    GT511C3_INVALIDATE_CONFIG(&lkg_device_config);

    if (status == TEE_SUCCESS) {
        GT511C3_INIT_COMMAND_FRAME(&cmd, GT511C3_CMD_CLOSE);
        status = gt511c3_send_cmd(&cmd, NULL);
    }

    return status;
#endif
}

/*
 * Command handlers
 */

static TEE_Result gt511c3_cmd_initialize(uint32_t param_types,
                    TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result status;
    uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_MEMREF_INOUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    _DMSG("gt511c3_cmd_initialize");

    if (exp_pt != param_types) {
        return TEE_ERROR_BAD_PARAMETERS;
    }

    status = gt511c3_open(
                (GT511C3_DeviceConfig *)params[0].memref.buffer,
                (GT511C3_DeviceInfo *)params[1].memref.buffer);

    return status;
}

static TEE_Result gt511c3_cmd_exec(uint32_t param_types,
                    TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result status;
    gt511c3_command cmd;


    if (TEE_PARAM_TYPE_GET(param_types, 0) != TEE_PARAM_TYPE_VALUE_INOUT) {
        return TEE_ERROR_BAD_PARAMETERS;
    }

    if (params[0].value.a == GT511C3_CMD_INVALID) {
        return TEE_ERROR_BAD_PARAMETERS;
    }
   
    GT511C3_INIT_COMMAND_FRAME(&cmd, params[0].value.a);
    cmd.parameter = params[0].value.b;

    _DMSG(
        "gt511c3_cmd_exec: cmd 0x%x, param %d, in size %d, outsize %d",
        params[0].value.a,
        params[0].value.b,
        params[1].memref.size,
        params[2].memref.size);

    status = gt511c3_send_cmd(&cmd, &params[0].value.b);
    if (status != TEE_SUCCESS) {
        EMSG(
            "cmd failed, status 0x%X, result 0x%X", 
            status, 
            params[0].value.b);

        return status;
    } else {
        _DMSG("cmd succeeded, result 0x%X",  params[0].value.b);
    }

    if (params[1].memref.size != 0) {
        status = gt511c3_send_data(
                     (uint8_t*)params[1].memref.buffer,
                     params[1].memref.size,
                     &params[0].value.b);

        if (status != TEE_SUCCESS) {
            EMSG("gt511c3_send_data failed, status 0x%X", status);
            return status;
        }
    }

    if (params[2].memref.size != 0) {
        status = gt511c3_recv_data(
                     (uint8_t*)params[2].memref.buffer,
                     params[2].memref.size);

        if (status != TEE_SUCCESS) {
            EMSG("gt511c3_recv_data failed, status 0x%X", status);
            return status;
        }
    }
    
    return TEE_SUCCESS;
}

/* 
 * Trusted Application Entry Points
 */

static TEE_Result pta_gt511c3_open_session(uint32_t param_types __unused,
                    TEE_Param params[TEE_NUM_PARAMS] __unused,
                    void **sess_ctx __unused)
{
    TEE_Result status;
    struct tee_ta_session *session;
    
    session = tee_ta_get_calling_session();
    
    /* Access is restricted to TA only */
    if (session == NULL) {
        EMSG("gt511c3 open session failed, REE access is not allowed!");
        return TEE_ERROR_ACCESS_DENIED;
    }

    /* 
     * Allow exclusive access only, acquire device 
     */

    { /* Locked op */
        uint32_t exceptions;

        exceptions = cpu_spin_lock_xsave(&gt511c3_lock);

        if ((curr_session != NULL) && (session != curr_session)) {
            status = TEE_ERROR_ACCESS_DENIED;
        } else {
            curr_session = session;
            status = TEE_SUCCESS;
        }
   
        cpu_spin_unlock_xrestore(&gt511c3_lock, exceptions);
    } /* Locked op */
    
    if (status != TEE_SUCCESS) {
        EMSG("gt511c3 open session failed, already opened (exclusive)!");
        return TEE_ERROR_ACCESS_DENIED;
    }
    
    _DMSG("gt511c3 open session succeeded!");
    return TEE_SUCCESS;
}

static void pta_gt511c3_close_session(void *sess_ctx __unused)
{
    TEE_Result status;

    status = gt511c3_close();
    if (status != TEE_SUCCESS) {
        EMSG("gt511c3 close failed, status 0x%X!", status);
    } else {
        _DMSG("gt511c3 close session succeeded!");
    }

    /* 
     * Clear active session, release device 
     */

    { /* Locked op */
        uint32_t exceptions;

        exceptions = cpu_spin_lock_xsave(&gt511c3_lock);

        curr_session = NULL;

        cpu_spin_unlock_xrestore(&gt511c3_lock, exceptions);
    } /* Locked op */
}

static TEE_Result pta_gt511c3_invoke_command(void *sess_ctx __unused, uint32_t cmd_id,
				 uint32_t param_types,
				 TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result res;
    
    _DMSG("gt511c3 invoke command %d\n", cmd_id);

    switch (cmd_id) {
    case PTA_GT511C3_INIT:
        res = gt511c3_cmd_initialize(param_types, params);
        break;

    case PTA_GT511C3_EXEC:
        res = gt511c3_cmd_exec(param_types, params);
        break;

    default:
        EMSG("Command not implemented %d", cmd_id);
        res = TEE_ERROR_NOT_IMPLEMENTED;
        break;
    }

    return res;
}

pseudo_ta_register(.uuid = PTA_GT511C3_UUID, .name = "pta_gt511c3",
    .flags = PTA_DEFAULT_FLAGS,
    .open_session_entry_point = pta_gt511c3_open_session,
    .close_session_entry_point = pta_gt511c3_close_session,
    .invoke_command_entry_point = pta_gt511c3_invoke_command);
