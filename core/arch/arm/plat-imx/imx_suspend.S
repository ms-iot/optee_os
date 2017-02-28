#include <arm32.h>
#include <arm32_macros.S>
#include <arm32_macros_cortex_a9.S>
#include <asm.S>
#include <kernel/tz_ssvce_def.h>
#include <kernel/unwind.h>
#include <platform_config.h>

.section .text
.balign 4
.code 32

.global imx_resume_start
imx_resume_start:

/*
 * Code to start running when the CPU wakes up.
 * void imx_resume (uint32_t gpio_base);
 */
FUNC imx_resume , :
UNWIND(	.fnstart)

    // We expect the GPIO physical base address to be in r0.
    // We do this so that we don't have to do an LDR (pc-relative) load
    // instruction, because this code must be location independent.
    
    // read gpio_dr register
    ldr     r1, [r0, #0]            // read gpio_dr register

    // following 3 lines will turn off LED then return
    //bic     r1, r1, #(1 << 2)
    //str     r1, [r0, #0]
    //b end
    
loop:
    eor     r1, r1, #(1 << 2)       // toggle bit 2 (onboard LED)
    str     r1, [r0, #0]            // write gpio_dr register
    
    mov     r2, #1
    lsl     r2, r2, #23             // load 2^23 in r2
delay:
    subs    r2, r2, #1
    bne     delay

    b loop

end:
    bx lr
	UNWIND(	.fnend)
END_FUNC imx_resume 

.global imx_resume_end
imx_resume_end:

