/*
 * Copyright (C) Microsoft. All rights reserved
 */

#ifndef __PTA_GT511C3_H
#define __PTA_GT511C3_H

/*
 * Interface to the GT511C3 pseudo-TA.
 */


// {BD7B756D-CAC7-4E8F-8189-C69A1A21DE85}
#define PTA_GT511C3_UUID { 0xbd7b756d, 0xcac7, 0x4e8f, { \
                           0x81, 0x89, 0xc6, 0x9a, 0x1a, 0x21, 0xde, 0x85 } }

/*
 * GT511C3 device configuration
 */
typedef struct {
    paddr_t uart_base_pa;
    uint32_t uart_clock_hz;
    uint32_t baud_rate;
} GT511C3_DeviceConfig ;

/*
 * GT511C3 device information
 */
#define GT511C3_SERIAL_NUMBER_SIZE 16

typedef struct {
    uint32_t firmware_version;
    uint32_t iso_area_max_size;
    uint8_t device_serial_number[GT511C3_SERIAL_NUMBER_SIZE];
} GT511C3_DeviceInfo;

/*
 * Description: Device initialization
 * 
 * [in]  memref[0]: Device configuration (GT511C3_DeviceConfig)
 * [out] memref[1]: Device information (GT511C3_DeviceInfo)
 */ 
#define PTA_GT511C3_INIT    0

/*
 * Description: Execute a command
 * 
 * [in]  value[0].a: GT511C3 command 
 * [in]  value[0].b: command parameter
 * [out] memref[1]: Response data
 */ 
#define PTA_GT511C3_EXEC    1

#endif /* __PTA_GT511C3_H */
