/**
 * startup_mspm0g3507.s — ARM Cortex-M0+ startup for MSPM0G3507
 *
 * Initializes the vector table, stack pointer, and jumps to main().
 * No data/bss initialization is strictly required for this simple
 * bare-metal application, but the Reset_Handler copies .data from
 * flash and zeroes .bss before calling main().
 *
 * MANUAL ONLY: This startup is based on general Cortex-M0+ knowledge.
 * Linker symbols come from mspm0g3507.ld.
 */

    .syntax unified
    .cpu    cortex-m0plus
    .thumb

    .section .vectors, "a"
    .align  2

    .globl  __Vectors
    .globl  __stack_top

__Vectors:
    .word   __stack_top          /*  0: Initial Stack Pointer (MSP) */
    .word   Reset_Handler        /*  1: Reset */
    .word   NMI_Handler          /*  2: NMI */
    .word   HardFault_Handler    /*  3: HardFault */
    .word   0                    /*  4: Reserved (MemManage on M3+) */
    .word   0                    /*  5: Reserved (BusFault on M3+) */
    .word   0                    /*  6: Reserved (UsageFault on M3+) */
    .word   0                    /*  7: Reserved */
    .word   0                    /*  8: Reserved */
    .word   0                    /*  9: Reserved */
    .word   0                    /* 10: Reserved */
    .word   SVC_Handler          /* 11: SVCall */
    .word   0                    /* 12: Reserved (DebugMon on M3+) */
    .word   0                    /* 13: Reserved */
    .word   PendSV_Handler       /* 14: PendSV */
    .word   SysTick_Handler      /* 15: SysTick */
    /* External interrupts 0-31 */
    .fill 32, 4, 0

    .section .text.Reset_Handler, "ax", %progbits
    .thumb_func
    .globl  Reset_Handler
    .type   Reset_Handler, %function
Reset_Handler:
    /* Initialize .data section (copy from flash to SRAM) */
    ldr     r0, =_sidata        /* Source: flash end of .text */
    ldr     r1, =_sdata         /* Dest:   start of .data in SRAM */
    ldr     r2, =_edata         /* End:    end of .data in SRAM */
    cmp     r1, r2
    beq     2f
1:  ldr     r3, [r0]            /* Load word from flash */
    str     r3, [r1]            /* Store word to SRAM */
    adds    r0, r0, #4
    adds    r1, r1, #4
    cmp     r1, r2
    bne     1b

2:  /* Zero .bss section */
    ldr     r1, =_sbss
    ldr     r2, =_ebss
    movs    r3, #0
    cmp     r1, r2
    beq     3f
3:  str     r3, [r1]
    adds    r1, r1, #4
    cmp     r1, r2
    bne     3b

    /* Call main() */
    bl      main

    /* If main() returns, loop forever */
    b       .

    .size   Reset_Handler, . - Reset_Handler

/*===========================================================================
 * Default exception handlers — infinite loop (trap)
 *===========================================================================*/
    .macro  DEFAULT_HANDLER name
    .section .text.\name, "ax", %progbits
    .thumb_func
    .weak   \name
    .type   \name, %function
\name:
    b       .
    .size   \name, . - \name
    .endm

    DEFAULT_HANDLER NMI_Handler
    DEFAULT_HANDLER HardFault_Handler
    DEFAULT_HANDLER SVC_Handler
    DEFAULT_HANDLER PendSV_Handler
    DEFAULT_HANDLER SysTick_Handler

    .end
