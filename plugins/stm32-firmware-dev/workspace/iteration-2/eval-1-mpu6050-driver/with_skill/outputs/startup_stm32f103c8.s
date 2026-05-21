/**
 * startup_stm32f103c8.s — Startup code for STM32F103C8T6 (Cortex-M3)
 *
 * Sets up the vector table, initializes .data and .bss, calls main().
 *
 * Vector table entries (simplified to save flash — only used handlers):
 *   Reset_Handler, NMI_Handler, HardFault_Handler,
 *   SysTick_Handler, Default_Handler (for all others)
 */

.syntax unified
.cpu cortex-m3
.thumb

/* Exported symbols */
.global _stack_end
.global Reset_Handler
.global Default_Handler
.global SysTick_Handler

/* Weak: applications can override */
.weak NMI_Handler
.weak HardFault_Handler
.weak MemManage_Handler
.weak BusFault_Handler
.weak UsageFault_Handler
.weak SVC_Handler
.weak DebugMon_Handler
.weak PendSV_Handler
.weak SysTick_Handler
.weak WWDG_IRQHandler
.weak PVD_IRQHandler
.weak TAMPER_IRQHandler
.weak RTC_IRQHandler
.weak FLASH_IRQHandler
.weak RCC_IRQHandler
.weak EXTI0_IRQHandler
.weak EXTI1_IRQHandler
.weak EXTI2_IRQHandler
.weak EXTI3_IRQHandler
.weak EXTI4_IRQHandler
.weak DMA1_Channel1_IRQHandler
.weak DMA1_Channel2_IRQHandler
.weak DMA1_Channel3_IRQHandler
.weak DMA1_Channel4_IRQHandler
.weak DMA1_Channel5_IRQHandler
.weak DMA1_Channel6_IRQHandler
.weak DMA1_Channel7_IRQHandler
.weak ADC1_2_IRQHandler
.weak USB_HP_CAN1_TX_IRQHandler
.weak USB_LP_CAN1_RX0_IRQHandler
.weak CAN1_RX1_IRQHandler
.weak CAN1_SCE_IRQHandler
.weak EXTI9_5_IRQHandler
.weak TIM1_BRK_IRQHandler
.weak TIM1_UP_IRQHandler
.weak TIM1_TRG_COM_IRQHandler
.weak TIM1_CC_IRQHandler
.weak TIM2_IRQHandler
.weak TIM3_IRQHandler
.weak TIM4_IRQHandler
.weak I2C1_EV_IRQHandler
.weak I2C1_ER_IRQHandler
.weak I2C2_EV_IRQHandler
.weak I2C2_ER_IRQHandler
.weak SPI1_IRQHandler
.weak SPI2_IRQHandler
.weak USART1_IRQHandler
.weak USART2_IRQHandler
.weak USART3_IRQHandler
.weak EXTI15_10_IRQHandler
.weak RTCAlarm_IRQHandler
.weak USBWakeUp_IRQHandler
.weak TIM8_BRK_IRQHandler
.weak TIM8_UP_IRQHandler
.weak TIM8_TRG_COM_IRQHandler
.weak TIM8_CC_IRQHandler
.weak ADC3_IRQHandler
.weak FSMC_IRQHandler
.weak SDIO_IRQHandler
.weak TIM5_IRQHandler
.weak SPI3_IRQHandler
.weak UART4_IRQHandler
.weak UART5_IRQHandler
.weak TIM6_IRQHandler
.weak TIM7_IRQHandler
.weak DMA2_Channel1_IRQHandler
.weak DMA2_Channel2_IRQHandler
.weak DMA2_Channel3_IRQHandler
.weak DMA2_Channel4_5_IRQHandler

/* External functions */
.extern main
.extern _sidata
.extern _sdata
.extern _edata
.extern _sbss
.extern _ebss

.section .isr_vector, "a", %progbits
.align 4

g_pfnVectors:
    .word _stack_end                 /* Initial stack pointer */
    .word Reset_Handler              /* Reset handler */
    .word NMI_Handler                /* NMI */
    .word HardFault_Handler          /* Hard fault */
    .word MemManage_Handler          /* MPU fault */
    .word BusFault_Handler           /* Bus fault */
    .word UsageFault_Handler         /* Usage fault */
    .word 0                          /* Reserved */
    .word 0                          /* Reserved */
    .word 0                          /* Reserved */
    .word 0                          /* Reserved */
    .word SVC_Handler                /* SVCall */
    .word DebugMon_Handler           /* Debug monitor */
    .word 0                          /* Reserved */
    .word PendSV_Handler             /* PendSV */
    .word SysTick_Handler            /* SysTick */
    /* External interrupts (60 entries for STM32F103 medium-density) */
    .word WWDG_IRQHandler
    .word PVD_IRQHandler
    .word TAMPER_IRQHandler
    .word RTC_IRQHandler
    .word FLASH_IRQHandler
    .word RCC_IRQHandler
    .word EXTI0_IRQHandler
    .word EXTI1_IRQHandler
    .word EXTI2_IRQHandler
    .word EXTI3_IRQHandler
    .word EXTI4_IRQHandler
    .word DMA1_Channel1_IRQHandler
    .word DMA1_Channel2_IRQHandler
    .word DMA1_Channel3_IRQHandler
    .word DMA1_Channel4_IRQHandler
    .word DMA1_Channel5_IRQHandler
    .word DMA1_Channel6_IRQHandler
    .word DMA1_Channel7_IRQHandler
    .word ADC1_2_IRQHandler
    .word USB_HP_CAN1_TX_IRQHandler
    .word USB_LP_CAN1_RX0_IRQHandler
    .word CAN1_RX1_IRQHandler
    .word CAN1_SCE_IRQHandler
    .word EXTI9_5_IRQHandler
    .word TIM1_BRK_IRQHandler
    .word TIM1_UP_IRQHandler
    .word TIM1_TRG_COM_IRQHandler
    .word TIM1_CC_IRQHandler
    .word TIM2_IRQHandler
    .word TIM3_IRQHandler
    .word TIM4_IRQHandler
    .word I2C1_EV_IRQHandler
    .word I2C1_ER_IRQHandler
    .word I2C2_EV_IRQHandler
    .word I2C2_ER_IRQHandler
    .word SPI1_IRQHandler
    .word SPI2_IRQHandler
    .word USART1_IRQHandler
    .word USART2_IRQHandler
    .word USART3_IRQHandler
    .word EXTI15_10_IRQHandler
    .word RTCAlarm_IRQHandler
    .word USBWakeUp_IRQHandler
    .word TIM8_BRK_IRQHandler
    .word TIM8_UP_IRQHandler
    .word TIM8_TRG_COM_IRQHandler
    .word TIM8_CC_IRQHandler
    .word ADC3_IRQHandler
    .word FSMC_IRQHandler
    .word SDIO_IRQHandler
    .word TIM5_IRQHandler
    .word SPI3_IRQHandler
    .word UART4_IRQHandler
    .word UART5_IRQHandler
    .word TIM6_IRQHandler
    .word TIM7_IRQHandler
    .word DMA2_Channel1_IRQHandler
    .word DMA2_Channel2_IRQHandler
    .word DMA2_Channel3_IRQHandler
    .word DMA2_Channel4_5_IRQHandler

.section .text.Reset_Handler, "ax", %progbits
.thumb_func
Reset_Handler:
    /* Copy .data section from flash to RAM */
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
    cmp r1, r2
    beq 2f
1:  ldr r3, [r0], #4
    str r3, [r1], #4
    cmp r1, r2
    bne 1b

2:  /* Zero .bss section */
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
    cmp r0, r1
    beq 4f
3:  str r2, [r0], #4
    cmp r0, r1
    bne 3b

4:  /* Call main() */
    bl main

    /* Should not reach here */
    b .

/* Default handler: infinite loop */
.section .text.Default_Handler, "ax", %progbits
.weak Default_Handler
.thumb_func
Default_Handler:
    b .
