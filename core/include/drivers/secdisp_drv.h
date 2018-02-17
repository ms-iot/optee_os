/*
* Copyright (C) Microsoft. All rights reserved
*/

#ifndef __DRIVERS_SECDISP_H
#define __DRIVERS_SECDISP_H

#include <assert.h>
#include <stdbool.h>
#include <types_ext.h>

struct secdisp_driver;

struct secdisp_ops {
    void (*deinit)(struct secdisp_driver *driver);
    
    TEE_Result (*draw_pixel)(
        struct secdisp_driver *driver, 
        int16_t x, 
        int16_t y, 
        uint16_t color);

    TEE_Result (*draw_line)(
        struct secdisp_driver *driver, 
        int16_t x, 
        int16_t y,
        int16_t length,
        uint16_t color,
        bool is_vertical);

    TEE_Result (*fill_rect)(
        struct secdisp_driver *driver,
        int16_t x,
        int16_t y,
        int16_t w,
        int16_t h,
        uint16_t color);

    TEE_Result (*set_rotation)(struct secdisp_driver *driver, uint8_t rotation);
    TEE_Result (*invert_display)(struct secdisp_driver *driver, bool is_invert);
    TEE_Result (*set_text_pos)(
        struct secdisp_driver *driver,
        int16_t x,
        int16_t y);

    TEE_Result (*draw_text)(
        struct secdisp_driver *driver,
        int16_t x,
        int16_t y,
        uint16_t color,
        uint8_t *text,
        uint16_t cont);
};

struct secdisp_driver {
    uint32_t status;
    const struct _SECDISP_INFORMATION* disp_info;
    const struct secdisp_ops *ops;
};

#endif /*__DRIVERS_SECDISP_H*/
