/* Startup code for STM32F103 (Cortex-M3)
 * Initializes data/bss, then calls main.  Includes bootstrap UART output
 * to trace execution flow in QEMU: '*' = reset, '.' = data copy done,
 * '#' = bss zeroed, then main() called.
 */

    .syntax unified
    .cpu cortex-m3
    .thumb

    .section .isr_vector, "a"
    .globl  __vector_table

__vector_table:
    .word   _estack                    /* Initial Stack Pointer */
    .word   Reset_Handler              /* Reset Handler */
    .word   NMI_Handler
    .word   HardFault_Handler
    .word   MemManage_Handler
    .word   BusFault_Handler
    .word   UsageFault_Handler
    .word   0                          /* Reserved */
    .word   0
    .word   0
    .word   0
    .word   SVC_Handler
    .word   DebugMon_Handler
    .word   0
    .word   PendSV_Handler
    .word   SysTick_Handler

    /* Remaining IRQ vector slots (zero-filled) */
    .fill 60, 4, 0

    .text
    .thumb_func
    .globl  Reset_Handler
    .type   Reset_Handler, %function

Reset_Handler:
    /* ---------------------------------------------------------------- */
    /* Bootstrap UART init: set up USART1 at 115200@8MHz, print '*'     */
    /* ---------------------------------------------------------------- */

    /* RCC_APB2ENR @ 0x40021018: set IOPAEN(bit2) | USART1EN(bit14) */
    ldr     r3, =0x40021018
    ldr     r2, [r3]
    ldr     r4, =((1 << 2) | (1 << 14))          /* 0x4004 */
    orr     r2, r2, r4
    str     r2, [r3]

    /* GPIOA_CRH @ 0x40010804: PA9 = AF PP 50MHz */
    /* PA9 uses CRH bits[7:4], set to 0xB (CNF=2=AF_PP, MODE=3=50MHz) */
    ldr     r3, =0x40010804
    ldr     r2, [r3]
    movs    r4, #0xF0                              /* mask for bits 7:4 */
    bics    r2, r2, r4                             /* clear bits[7:4] */
    movs    r4, #0xB0                              /* value 0xB << 4 */
    orrs    r2, r2, r4                             /* set bits[7:4] = 0xB */
    str     r2, [r3]

    /* USART1_BRR @ 0x40013808: 115200 baud */
    ldr     r3, =0x40013808
    movs    r2, #0x45
    str     r2, [r3]

    /* USART1_CR1 @ 0x4001380C: UE(bit13) | TE(bit3) */
    ldr     r3, =0x4001380C
    ldr     r2, =((1 << 13) | (1 << 3))           /* 0x2008 */
    str     r2, [r3]

    .ltorg

    /* Print '*' via USART1 */
    ldr     r3, =0x40013800                        /* USART1_SR */
1:  ldr     r2, [r3]
    ldr     r4, =0x80                              /* TXE = bit 7 */
    tst     r2, r4
    beq     1b
    ldr     r3, =0x40013804                        /* USART1_DR */
    mov     r2, #'*'
    str     r2, [r3]

    .ltorg

    /* ---------------------------------------------------------------- */
    /* Copy .data from FLASH to RAM                                      */
    /* ---------------------------------------------------------------- */
    ldr     r0, =_sdata
    ldr     r1, =_edata
    ldr     r2, =_etext
    cmp     r0, r1
    beq     zero_bss

copy_data:
    ldr     r3, [r2], #4
    str     r3, [r0], #4
    cmp     r0, r1
    bne     copy_data

    .ltorg

    /* Print '.' after data copy */
    ldr     r3, =0x40013800
1:  ldr     r2, [r3]
    movs    r4, #0x80
    tst     r2, r4
    beq     1b
    ldr     r3, =0x40013804
    mov     r2, #'.'
    str     r2, [r3]

    .ltorg

zero_bss:
    /* Zero the .bss section */
    ldr     r0, =_sbss
    ldr     r1, =_ebss
    movs    r2, #0
    cmp     r0, r1
    beq     call_main

zero_bss_loop:
    str     r2, [r0], #4
    cmp     r0, r1
    bne     zero_bss_loop

    .ltorg

    /* Print '#' after BSS zeroing */
    ldr     r3, =0x40013800
1:  ldr     r2, [r3]
    movs    r4, #0x80
    tst     r2, r4
    beq     1b
    ldr     r3, =0x40013804
    mov     r2, #'#'
    str     r2, [r3]

    .ltorg

call_main:
    bl      main

dead_loop:
    wfi
    b       dead_loop

    .size   Reset_Handler, . - Reset_Handler
    .end
