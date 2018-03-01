/*
* Copyright (C) Microsoft. All rights reserved
*/

#ifndef __SECDISP_LIB_H
#define __SECDISP_LIB_H

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <types_ext.h>
#include <string.h>

#include <pta_secdisp.h>

TEE_Result secdisp_init(SECDISP_DRIVERS driver, SECDISP_INFORMATION* disp_info);
void secdisp_deinit(void);

TEE_Result secdisp_clear(SECDISP_COLORS color);
TEE_Result secdisp_draw_pixel(int16_t x, int16_t y, SECDISP_COLORS color);
TEE_Result secdisp_draw_line(
    int16_t x,
    int16_t y,
    int16_t size,
    SECDISP_COLORS color,
    bool is_vertical);

TEE_Result secdisp_fill_rect(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h,
    SECDISP_COLORS color);

TEE_Result secdisp_set_rotation(SECDISP_ROTATION rotation);
TEE_Result secdisp_invert_display(bool is_invert);
TEE_Result secdisp_set_text_attr(
    SECDISP_COLORS text_color,
    SECDISP_COLORS background_color,
    uint16_t text_size);

TEE_Result secdisp_write_text(
    int16_t x, 
    int16_t y, 
    const char *text, 
    uint16_t count);

void secdisp_printf(const char *fmt, ...) __printf(1, 2);


/*
 * ESC characters (terminal emulation)
 */

/* 
 * Cursor 
 */
#define ESC_HOME "\x1B[H" /* Move cursor to home (0,0) position */

/*
 * Erasing text
 */
#define ESC_EL  "\x1B[0K" /* Clear line from current cursor position to end of line*/
#define ESC_EL1 "\x1B[1K" /* Clear line from beginning to current cursor position */
#define ESC_EL2 "\x1B[2K" /* Clear whole */

#endif /* __SECDISP_LIB_H */
