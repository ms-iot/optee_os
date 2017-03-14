#include <arm32.h>
#include <arm32_macros.S>
#include <arm32_macros_cortex_a9.S>
#include <asm.S>
#include <kernel/tz_ssvce_def.h>
#include <kernel/unwind.h>
#include <platform_config.h>
#include <imx_suspend.h>

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

/*
 * bool save_state_for_suspend (struct armv7_processor_state* state);
 *
 * Returns FALSE when spending, returns TRUE when resuming.
 */
FUNC save_state_for_suspend , :
    UNWIND(	.fnstart)
    push    {r4, lr}

    // XXX need to disable interrupts?

    // compute address of context structure
    mov     r4, r0
    add     r0, r0, #ARMV7_PROCESSOR_STATE_CONTEXT_OFFSET
    bl      armv7_capture_context
    
    // fixup captured r0, sp, lr, and pc so that when context is restored,
    // execution resumes at the call site of this function,
    // with true in r0
    add     r0, r4, #ARMV7_PROCESSOR_STATE_CONTEXT_OFFSET

    // store 'true' into context record's r0
    mov     r1, #1
    str     r1, [r0, #ARMV7_CONTEXT_R0_OFFSET]
    
    // adjust context record's stack pointer to its value on entry
    add     r1, sp, #8
    str     r1, [r0, #ARMV7_CONTEXT_SP_OFFSET]

    // we want execution to resume at lr
    ldr     r1, [sp, #0]                        // load saved value of lr
    str     r1, [r0, #ARMV7_CONTEXT_PC_OFFSET]
    
    // lr is a callee-saved register, so it must be restored to its
    // value on entry
    str     r1, [r0, #ARMV7_CONTEXT_LR_OFFSET]

    // restore the caller-saved register r4 which we saved on the stack
    ldr     r1, [sp, #4]                        // load saved value of r4
    str     r1, [r0, #ARMV7_CONTEXT_R4_OFFSET]

    // Save architectural state
    add     r0, r4, #ARMV7_PROCESSOR_STATE_ARCH_STATE_OFFSET
    bl      armv7_save_arch_state
    
    // save special registers
    add     r0, r4, #ARMV7_PROCESSOR_STATE_SPECIAL_REGISTERS_OFFSET
    bl      armv7_save_special_regs    

    // return false in suspend path
    mov     r0, #0

    pop     {r4, lr}
    bx      lr 

	UNWIND(	.fnend)
END_FUNC save_state_for_suspend

/*
 * void armv7_capture_context (struct arm7_context* context);
 */
FUNC armv7_capture_context , :
    UNWIND( .fnstart)
    
    stm     r0, {r0-r14}
    str     lr, [r0, #ARMV7_CONTEXT_PC_OFFSET]
    mrs     r1, cpsr
    str     r1, [r0, #ARMV7_CONTEXT_CPSR_OFFSET]
    bx      lr

    UNWIND( .fnend)
END_FUNC armv7_capture_context

/*
 * void armv7_save_special_regs (struct armv7_special_registers* state);
 */
FUNC armv7_save_special_regs , :
    UNWIND( .fnstart)
    
    //mrc     p15, 0, r1, c13, c0, 2   // TPIDRURW
    read_tpidrurw r1
    str     r1, [r0, #ARMV7_SPECIAL_REGISTERS_CP15_TPIDRURW_OFFSET]
    
    //mrc     p15, 0, r1, c13, c0, 3   // TPIDRURO
    read_tpidruro r1
    str     r1, [r0, #ARMV7_SPECIAL_REGISTERS_CP15_TPIDRURO_OFFSET]
    
    //mrc     p15, 0, r1, c13, c0, 4   // TPIDRPRW
    read_tpidrprw r1
    str     r1, [r0, #ARMV7_SPECIAL_REGISTERS_CP15_TPIDRPRW_OFFSET]

    // save banked registers
    //yyy cps     # 

    bx      lr
    UNWIND( .fnend)
END_FUNC armv7_save_special_regs 


