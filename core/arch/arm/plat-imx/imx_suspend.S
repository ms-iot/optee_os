#include <arm.h>
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
    
    // toggle state of LED
    //ldr     r1, [r0, #0]            // read gpio_dr register
    //eor     r1, r1, #(1 << 2)       // toggle bit 2 (onboard LED)
    //str     r1, [r0, #0]

    cpsid   if                      // disable interrupts

    // XXX are all the ISB's necessary? Should not be necessary until MMU
    // and branch prediction are enabled

    // restore DACR
    ldr     r1, [r0, #(ARMV7_PROCESSOR_STATE_ARCH_STATE_OFFSET + \
                       ARMV7_ARCH_STATE_CP15_DACR_OFFSET)]
    write_dacr  r1
    isb

    mov     r1, #0xff               // TRANSITION_ASID XXX need to restore actual value?
    dsb
    write_contextidr    r1
    isb

    // Restore TTBCR
    movw    r2, #(ARMV7_PROCESSOR_STATE_ARCH_STATE_OFFSET + \
                  ARMV7_ARCH_STATE_CP15_TTBCR_OFFSET)

    ldr     r1, [r0, r2]
    write_ttbcr r1
 
    // Restore TTBR0
    movw    r2, #(ARMV7_PROCESSOR_STATE_ARCH_STATE_OFFSET + \
                  ARMV7_ARCH_STATE_CP15_TTBR0_OFFSET)

    ldr     r1, [r0, r2]
    write_ttbr0 r1

    // Restore TTBR1
    movw    r2, #(ARMV7_PROCESSOR_STATE_ARCH_STATE_OFFSET + \
                  ARMV7_ARCH_STATE_CP15_TTBR1_OFFSET)

    ldr     r1, [r0, r2]
    write_ttbr1 r1
    
    // Restore NMRR
    movw    r2, #(ARMV7_PROCESSOR_STATE_ARCH_STATE_OFFSET + \
                  ARMV7_ARCH_STATE_CP15_NMRR_OFFSET)

    ldr     r1, [r0, r2]
    write_nmrr r1

    // Restore PRRR
    movw    r2, #(ARMV7_PROCESSOR_STATE_ARCH_STATE_OFFSET + \
                  ARMV7_ARCH_STATE_CP15_PRRR_OFFSET)

    ldr     r1, [r0, r2]
    write_prrr r1
    
    // Restore TPIDRURW
    //movw    r2, #(ARMV7_PROCESSOR_STATE_SPECIAL_REGISTERS_OFFSET + \
    //              ARMV7_SPECIAL_REGISTERS_CP15_TPIDRURW_OFFSET)

    //ldr     r1, [r0, r2]
    //write_tpidrurw r1
 
    // Restore TPIDRURO
    //movw    r2, #(ARMV7_PROCESSOR_STATE_SPECIAL_REGISTERS_OFFSET + \
    //              ARMV7_SPECIAL_REGISTERS_CP15_TPIDRURO_OFFSET)

    //ldr     r1, [r0, r2]
    //write_tpidruro r1
 
    // Restore TPIDRPRW
    //movw    r2, #(ARMV7_PROCESSOR_STATE_SPECIAL_REGISTERS_OFFSET + \
    //              ARMV7_SPECIAL_REGISTERS_CP15_TPIDRPRW_OFFSET)

    //ldr     r1, [r0, r2]
    //write_tpidrprw r1

    mov     r4, r0
    
//    movw    r0, #0xC000             // load GPIO physical address to r0
//    movt    r0, #0x0209
//    bl      blink_led
//    mov     r0, r4 

    // load gpio virtual address to r5
    mov     r2, #0xe8       // gpio_virt_base
    ldr     r5, [r0, r2]

    // Load SCTLR value from memory
    movw    r2, #(ARMV7_PROCESSOR_STATE_ARCH_STATE_OFFSET + \
                  ARMV7_ARCH_STATE_CP15_SCTLR_OFFSET)

    ldr     r1, [r0, r2]

    // Load virtual address of processor_state structure to r6
    mov     r2, #0xec       // resume_state_virt_base
    ldr     r6, [r0, r2]

    write_tlbiall                   // invalidate tlb
    dsb
    isb
    
    write_iciallu                   // invalidate icache
    dsb
    isb

    write_bpiall                    // invalidate btac
    dsb
    isb

    write_sctlr r1
    dsb
    isb

    mov     r0, r5      // gpio_virt_base
    bl      blink_led

    // restore banked registers
    add     r0, r6, #ARMV7_PROCESSOR_STATE_SPECIAL_REGISTERS_OFFSET
    bl      armv7_restore_special_regs

    // restore context
    add     r0, r6, #ARMV7_PROCESSOR_STATE_CONTEXT_OFFSET
    bl      armv7_restore_context

    // we should never actually get here

    mov     r0, r5      // gpio_virt_base
    bl      blink_led

end:
    bx lr
	UNWIND(	.fnend)
END_FUNC imx_resume 

/*
 * void blink_led(uint32_t gpio_base)
 */
FUNC blink_led , :
    UNWIND( .fnstart)

    //movw    r0, #0xC000             // load GPIO physical address to r0
    //movt    r0, #0x0209
  
    ldr     r1, [r0, #0]            // load gpio_dr register
 
loop:
    eor     r1, r1, #(1 << 2)       // toggle bit 2 (onboard LED)
    str     r1, [r0, #0]            // write gpio_dr register
    
    mov     r2, #1
    lsl     r2, r2, #21             // load 2^21 in r2
delay:
    subs    r2, r2, #1
    bne     delay

    b       loop

    bx lr

	UNWIND( .fnend)
END_FUNC blink_led


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

    mov     r4, r0

    // XXX need to disable interrupts?
    mov     r2, #0xe8       // gpio_virt_base
    ldr     r0, [r4, r2]
    bl      blink_led 

    // compute address of context structure
//    mov     r4, r0
    mov     r0, r4
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
    ldr     r1, [sp, #4]                        // load saved value of lr
    str     r1, [r0, #ARMV7_CONTEXT_PC_OFFSET]
    
    // lr is a callee-saved register, so it must be restored to its
    // value on entry
    str     r1, [r0, #ARMV7_CONTEXT_LR_OFFSET]

    // restore the caller-saved register r4 which we saved on the stack
    ldr     r1, [sp, #0]                        // load saved value of r4
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
 * void armv7_restore_context (struct arm7_context* context);
 *
 * Restores context from the context record. Resumes execution at the
 * pc value stored in the context record.
 */
FUNC armv7_restore_context , :
    UNWIND( .fnstart)
    
    ldr     r1, [r0, #ARMV7_CONTEXT_CPSR_OFFSET]
    msr     cpsr, r1
    ldm     r0, {r0-r14, pc}
    
    // should never actually reach here
l1:
    wfi
    b      l1

    UNWIND( .fnend)
END_FUNC armv7_restore_context


/*
 * void armv7_save_special_regs (struct armv7_special_registers* state);
 *
 * Must be running in monitor mode and IRQ/FIQ must be disabled.
 */
FUNC armv7_save_special_regs , :
    UNWIND( .fnstart)
   
    // XXX assert precondition: current mode must be monitor mode
 
    read_tpidrurw r1
    str     r1, [r0, #ARMV7_SPECIAL_REGISTERS_CP15_TPIDRURW_OFFSET]
    
    read_tpidruro r1
    str     r1, [r0, #ARMV7_SPECIAL_REGISTERS_CP15_TPIDRURO_OFFSET]
    
    read_tpidrprw r1
    str     r1, [r0, #ARMV7_SPECIAL_REGISTERS_CP15_TPIDRPRW_OFFSET]

    add     r0, r0, #ARMV7_SPECIAL_REGISTERS_USR_SP_OFFSET
    
    // save banked registers
    cps     #CPSR_MODE_SYS
    stm     r0!, {sp, lr}

    cps     #CPSR_MODE_IRQ
    mrs     r2, spsr
    stm     r0!, {r2, sp, lr}

    cps     #CPSR_MODE_FIQ
    mrs     r2, spsr
    stm     r0!, {r2, r8-r12, sp, lr}

    cps     #CPSR_MODE_SVC
    mrs     r2, spsr
    stm     r0!, {r2, sp, lr}

    cps     #CPSR_MODE_ABT
    mrs     r2, spsr
    stm     r0!, {r2, sp, lr}

    cps     #CPSR_MODE_UND
    mrs     r2, spsr
    stm     r0!, {r2, sp, lr}

    cps     #CPSR_MODE_MON 
    mrs     r2, spsr
    stm     r0!, {r2} 

    bx      lr
    UNWIND( .fnend)
END_FUNC armv7_save_special_regs 

/*
 * void armv7_restore_special_regs (struct armv7_special_registers* state);
 *
 * Must be running in monitor mode and IRQ/FIQ must be disabled.
 */
FUNC armv7_restore_special_regs , :
    UNWIND( .fnstart)
   
    // XXX assert precondition: current mode must be monitor mode
    
    // just in case we're not in monitor mode, make sure we return
    // with the same sp and lr we entered with
    // XXX remove this code
    mov     r2, sp
    mov     r3, lr

    ldr     r1, [r0, #ARMV7_SPECIAL_REGISTERS_CP15_TPIDRURW_OFFSET]
    write_tpidrurw r1
    
    ldr     r1, [r0, #ARMV7_SPECIAL_REGISTERS_CP15_TPIDRURO_OFFSET]
    write_tpidruro r1
    
    ldr     r1, [r0, #ARMV7_SPECIAL_REGISTERS_CP15_TPIDRPRW_OFFSET]
    write_tpidrprw r1

    add     r0, r0, #ARMV7_SPECIAL_REGISTERS_USR_SP_OFFSET
    
    // restore banked registers
    cps     #CPSR_MODE_SYS
    ldm     r0!, {sp, lr}

    cps     #CPSR_MODE_IRQ
    ldm     r0!, {r1, sp, lr}
    msr     spsr, r1

    cps     #CPSR_MODE_FIQ
    ldm     r0!, {r1, r8-r12, sp, lr}
    msr     spsr, r1

    cps     #CPSR_MODE_SVC
    ldm     r0!, {r1, sp, lr}
    msr     spsr, r1

    cps     #CPSR_MODE_ABT
    ldm     r0!, {r1, sp, lr}
    msr     spsr, r1

    cps     #CPSR_MODE_UND
    ldm     r0!, {r1, sp, lr}
    msr     spsr, r1

    cps     #CPSR_MODE_MON 
    ldr     r1, [r0]
    msr     spsr, r1

    // XXX this shouldn't be necessary if we're already running in
    // monitor mode
    mov     sp, r2
    mov     lr, r3

    bx      lr
    UNWIND( .fnend)
END_FUNC armv7_restore_special_regs 


