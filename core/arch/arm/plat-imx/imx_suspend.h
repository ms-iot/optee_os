#ifndef _IMX_SUSPEND_H_
#define _IMX_SUSPEND_H_

#ifndef ASM

struct armv7_context {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t r12;
    uint32_t sp;
    uint32_t lr;
    uint32_t pc;
    uint32_t cpsr;

    // XXX floating point/NEON registers
    // XXX debug registers
};

struct armv7_special_registers {
    uint32_t cp15_tpidrurw;
    uint32_t cp15_tpidruro;
    uint32_t cp15_tpidrprw;

    // XXX H/w [break/watch]point support.
    // XXX floating point control state

    //
    // Banked general purpose registers.
    //
    uint32_t usr_sp;
    uint32_t usr_lr;

    uint32_t irq_spsr;
    uint32_t irq_sp;
    uint32_t irq_lr;

    uint32_t fiq_spsr;
    uint32_t fiq_r8;
    uint32_t fiq_r9;
    uint32_t fiq_r10;
    uint32_t fiq_r11;
    uint32_t fiq_r12;
    uint32_t fiq_sp;
    uint32_t fiq_lr;

    uint32_t svc_spsr;
    uint32_t svc_sp;
    uint32_t svc_lr;

    uint32_t abt_spsr;
    uint32_t abt_sp;
    uint32_t abt_lr;

    uint32_t undef_spsr;
    uint32_t undef_sp;
    uint32_t undef_lr;

    uint32_t mon_spsr;
};

struct armv7_arch_state {
    // uint32_t Cp15_Cr0_armv7Id;
    uint32_t cp15_sctlr;
    uint32_t cp15_actlr;
    uint32_t cp15_cpacr;
    uint32_t cp15_ttbcr;
    uint32_t cp15_ttbr0;
    uint32_t cp15_ttbr1;
    uint32_t cp15_dacr;
    uint32_t cp15_dfsr;
    uint32_t cp15_ifsr;
    uint32_t cp15_dfar;
    uint32_t cp15_ifar;
    // XXX need to save PM state?
    // uint32_t Cp15_Cr9_PmControl;
    // uint32_t Cp15_Cr9_PmCountEnableSet;
    // uint32_t Cp15_Cr9_PmCycleCounter;
    // uint32_t Cp15_Cr9_PmEventCounter[MAX_EVENT_COUNTERS];
    // uint32_t Cp15_Cr9_PmEventType[MAX_EVENT_COUNTERS];
    // uint32_t Cp15_Cr9_PmInterruptSelect;
    // uint32_t Cp15_Cr9_PmOverflowStatus;
    // uint32_t Cp15_Cr9_PmSelect;
    // uint32_t Cp15_Cr9_PmUserEnable;
    uint32_t cp15_prrr;
    uint32_t cp15_nmrr;
    uint32_t cp15_vbar;
    uint32_t cp15_contextidr;
};

struct armv7_processor_state {
    struct armv7_special_registers special_registers;
    struct armv7_arch_state arch_state;
    struct armv7_context context;
    uint32_t gpio_virt_base;
    uint32_t resume_state_virt_base;
};

bool save_state_for_suspend (struct armv7_processor_state* state);

void armv7_capture_context (struct armv7_context* context);

void armv7_save_arch_state (struct armv7_arch_state* state);

void imx_resume (uint32_t r0);
extern uint8_t imx_resume_start;
extern uint8_t imx_resume_end;

void imx_psci_init (void);

#endif /* ASM */

#define ARMV7_PROCESSOR_STATE_SPECIAL_REGISTERS_OFFSET 0x0
#define ARMV7_PROCESSOR_STATE_ARCH_STATE_OFFSET 0x68
#define ARMV7_PROCESSOR_STATE_CONTEXT_OFFSET 0xa4

#define ARMV7_CONTEXT_R0_OFFSET 0x0
#define ARMV7_CONTEXT_R1_OFFSET 0x4
#define ARMV7_CONTEXT_R2_OFFSET 0x8
#define ARMV7_CONTEXT_R3_OFFSET 0xC
#define ARMV7_CONTEXT_R4_OFFSET 0x10
#define ARMV7_CONTEXT_R5_OFFSET 0x14
#define ARMV7_CONTEXT_R6_OFFSET 0x18
#define ARMV7_CONTEXT_R7_OFFSET 0x1C
#define ARMV7_CONTEXT_R8_OFFSET 0x20
#define ARMV7_CONTEXT_R9_OFFSET 0x24
#define ARMV7_CONTEXT_R10_OFFSET 0x28
#define ARMV7_CONTEXT_R11_OFFSET 0x2C
#define ARMV7_CONTEXT_R12_OFFSET 0x30
#define ARMV7_CONTEXT_SP_OFFSET 0x34
#define ARMV7_CONTEXT_LR_OFFSET 0x38
#define ARMV7_CONTEXT_PC_OFFSET 0x3C
#define ARMV7_CONTEXT_CPSR_OFFSET 0x40

#define ARMV7_SPECIAL_REGISTERS_CP15_TPIDRURW_OFFSET 0x0
#define ARMV7_SPECIAL_REGISTERS_CP15_TPIDRURO_OFFSET 0x4
#define ARMV7_SPECIAL_REGISTERS_CP15_TPIDRPRW_OFFSET 0x8
#define ARMV7_SPECIAL_REGISTERS_USR_SP_OFFSET 0xC
#define ARMV7_SPECIAL_REGISTERS_USR_LR_OFFSET 0x10
#define ARMV7_SPECIAL_REGISTERS_IRQ_SPSR_OFFSET 0x14
#define ARMV7_SPECIAL_REGISTERS_IRQ_SP_OFFSET 0x18
#define ARMV7_SPECIAL_REGISTERS_IRQ_LR_OFFSET 0x1C
#define ARMV7_SPECIAL_REGISTERS_FIQ_SPSR_OFFSET 0x20
#define ARMV7_SPECIAL_REGISTERS_FIQ_R8_OFFSET 0x24
#define ARMV7_SPECIAL_REGISTERS_FIQ_R9_OFFSET 0x28
#define ARMV7_SPECIAL_REGISTERS_FIQ_R10_OFFSET 0x2C
#define ARMV7_SPECIAL_REGISTERS_FIQ_R11_OFFSET 0x30
#define ARMV7_SPECIAL_REGISTERS_FIQ_R12_OFFSET 0x34
#define ARMV7_SPECIAL_REGISTERS_FIQ_SP_OFFSET 0x38
#define ARMV7_SPECIAL_REGISTERS_FIQ_LR_OFFSET 0x3C
#define ARMV7_SPECIAL_REGISTERS_SVC_SPSR_OFFSET 0x40
#define ARMV7_SPECIAL_REGISTERS_SVC_SP_OFFSET 0x44
#define ARMV7_SPECIAL_REGISTERS_SVC_LR_OFFSET 0x48
#define ARMV7_SPECIAL_REGISTERS_ABT_SPSR_OFFSET 0x4C
#define ARMV7_SPECIAL_REGISTERS_ABT_SP_OFFSET 0x50
#define ARMV7_SPECIAL_REGISTERS_ABT_LR_OFFSET 0x54
#define ARMV7_SPECIAL_REGISTERS_UNDEF_SPSR_OFFSET 0x58
#define ARMV7_SPECIAL_REGISTERS_UNDEF_SP_OFFSET 0x5C
#define ARMV7_SPECIAL_REGISTERS_UNDEF_LR_OFFSET 0x60
#define ARMV7_SPECIAL_REGISTERS_MON_SPSR_OFFSET 0x64

#define ARMV7_ARCH_STATE_CP15_SCTLR_OFFSET 0x0
#define ARMV7_ARCH_STATE_CP15_ACTLR_OFFSET 0x4
#define ARMV7_ARCH_STATE_CP15_CPACR_OFFSET 0x8
#define ARMV7_ARCH_STATE_CP15_TTBCR_OFFSET 0xC
#define ARMV7_ARCH_STATE_CP15_TTBR0_OFFSET 0x10
#define ARMV7_ARCH_STATE_CP15_TTBR1_OFFSET 0x14
#define ARMV7_ARCH_STATE_CP15_DACR_OFFSET 0x18
#define ARMV7_ARCH_STATE_CP15_DFSR_OFFSET 0x1C
#define ARMV7_ARCH_STATE_CP15_IFSR_OFFSET 0x20
#define ARMV7_ARCH_STATE_CP15_DFAR_OFFSET 0x24
#define ARMV7_ARCH_STATE_CP15_IFAR_OFFSET 0x28
#define ARMV7_ARCH_STATE_CP15_PRRR_OFFSET 0x2C
#define ARMV7_ARCH_STATE_CP15_NMRR_OFFSET 0x30
#define ARMV7_ARCH_STATE_CP15_VBAR_OFFSET 0x34
#define ARMV7_ARCH_STATE_CP15_CONTEXTIDR_OFFSET 0x38

#endif /* _IMX_SUSPEND_H_ */
