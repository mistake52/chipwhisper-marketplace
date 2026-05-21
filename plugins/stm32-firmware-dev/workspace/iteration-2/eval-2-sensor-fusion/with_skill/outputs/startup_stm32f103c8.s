/**
 * startup_stm32f103c8.s — Startup file for STM32F103C8 (Cortex-M3)
 *
 * Vector table layout (Cortex-M3, RM0008 Rev 21, Section 10.1.2):
 *   Word 0: Initial stack pointer
 *   Word 1: Reset vector (Reset_Handler)
 *   Words 2-15: System exceptions (NMI, HardFault, MemManage, BusFault, UsageFault,
 *                reserved, reserved, reserved, SVCall, DebugMonitor, reserved,
 *                PendSV, SysTick)
 *   Words 16+: IRQ handlers (USART1, USART2, I2C1, SPI1, etc.)
 */

    .syntax unified
    .cpu cortex-m3
    .thumb

/* Export symbols */
    .global g_pfnVectors
    .global Default_Handler

    .global Reset_Handler
    .global NMI_Handler
    .global HardFault_Handler
    .global MemManage_Handler
    .global BusFault_Handler
    .global UsageFault_Handler
    .global SVC_Handler
    .global DebugMon_Handler
    .global PendSV_Handler
    .global SysTick_Handler

/* IRQ handlers */
    .global WWDG_IRQHandler
    .global PVD_IRQHandler
    .global TAMPER_IRQHandler
    .global RTC_IRQHandler
    .global FLASH_IRQHandler
    .global RCC_IRQHandler
    .global EXTI0_IRQHandler
    .global EXTI1_IRQHandler
    .global EXTI2_IRQHandler
    .global EXTI3_IRQHandler
    .global EXTI4_IRQHandler
    .global DMA1_Channel1_IRQHandler
    .global DMA1_Channel2_IRQHandler
    .global DMA1_Channel3_IRQHandler
    .global DMA1_Channel4_IRQHandler
    .global DMA1_Channel5_IRQHandler
    .global DMA1_Channel6_IRQHandler
    .global DMA1_Channel7_IRQHandler
    .global ADC1_2_IRQHandler
    .global USB_HP_CAN1_TX_IRQHandler
    .global USB_LP_CAN1_RX0_IRQHandler
    .global CAN1_RX1_IRQHandler
    .global CAN1_SCE_IRQHandler
    .global EXTI9_5_IRQHandler
    .global TIM1_BRK_IRQHandler
    .global TIM1_UP_IRQHandler
    .global TIM1_TRG_COM_IRQHandler
    .global TIM1_CC_IRQHandler
    .global TIM2_IRQHandler
    .global TIM3_IRQHandler
    .global TIM4_IRQHandler
    .global I2C1_EV_IRQHandler
    .global I2C1_ER_IRQHandler
    .global I2C2_EV_IRQHandler
    .global I2C2_ER_IRQHandler
    .global SPI1_IRQHandler
    .global SPI2_IRQHandler
    .global USART1_IRQHandler
    .global USART2_IRQHandler
    .global USART3_IRQHandler
    .global EXTI15_10_IRQHandler
    .global RTC_Alarm_IRQHandler
    .global USBWakeUp_IRQHandler

    .section .isr_vector, "a"
    .type g_pfnVectors, %object
    .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0  /* reserved */
    .word 0  /* reserved */
    .word 0  /* reserved */
    .word 0  /* reserved */
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0  /* reserved */
    .word PendSV_Handler
    .word SysTick_Handler

    /* External interrupts */
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
    .word RTC_Alarm_IRQHandler
    .word USBWakeUp_IRQHandler

    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    ldr sp, =_estack

    /* Copy .data from flash to RAM */
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
    cmp r1, r2
    bge 2f
1:  ldr r3, [r0], #4
    str r3, [r1], #4
    cmp r1, r2
    blt 1b

    /* Zero .bss */
2:  ldr r1, =_sbss
    ldr r2, =_ebss
    mov r3, #0
    cmp r1, r2
    bge 4f
3:  str r3, [r1], #4
    cmp r1, r2
    blt 3b

    /* Call main */
4:  bl main
    b .  /* Trap if main returns */

    .section .text.Default_Handler
    .weak Default_Handler
    .type Default_Handler, %function
Default_Handler:
    b .

    /* Weak aliases for all handlers */
    .weak NMI_Handler
    .thumb_set NMI_Handler, Default_Handler

    .weak HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler

    .weak MemManage_Handler
    .thumb_set MemManage_Handler, Default_Handler

    .weak BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler

    .weak UsageFault_Handler
    .thumb_set UsageFault_Handler, Default_Handler

    .weak SVC_Handler
    .thumb_set SVC_Handler, Default_Handler

    .weak DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler

    .weak PendSV_Handler
    .thumb_set PendSV_Handler, Default_Handler

    .weak SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler

    .weak WWDG_IRQHandler
    .thumb_set WWDG_IRQHandler, Default_Handler

    .weak PVD_IRQHandler
    .thumb_set PVD_IRQHandler, Default_Handler

    .weak TAMPER_IRQHandler
    .thumb_set TAMPER_IRQHandler, Default_Handler

    .weak RTC_IRQHandler
    .thumb_set RTC_IRQHandler, Default_Handler

    .weak FLASH_IRQHandler
    .thumb_set FLASH_IRQHandler, Default_Handler

    .weak RCC_IRQHandler
    .thumb_set RCC_IRQHandler, Default_Handler

    .weak EXTI0_IRQHandler
    .thumb_set EXTI0_IRQHandler, Default_Handler

    .weak EXTI1_IRQHandler
    .thumb_set EXTI1_IRQHandler, Default_Handler

    .weak EXTI2_IRQHandler
    .thumb_set EXTI2_IRQHandler, Default_Handler

    .weak EXTI3_IRQHandler
    .thumb_set EXTI3_IRQHandler, Default_Handler

    .weak EXTI4_IRQHandler
    .thumb_set EXTI4_IRQHandler, Default_Handler

    .weak DMA1_Channel1_IRQHandler
    .thumb_set DMA1_Channel1_IRQHandler, Default_Handler

    .weak DMA1_Channel2_IRQHandler
    .thumb_set DMA1_Channel2_IRQHandler, Default_Handler

    .weak DMA1_Channel3_IRQHandler
    .thumb_set DMA1_Channel3_IRQHandler, Default_Handler

    .weak DMA1_Channel4_IRQHandler
    .thumb_set DMA1_Channel4_IRQHandler, Default_Handler

    .weak DMA1_Channel5_IRQHandler
    .thumb_set DMA1_Channel5_IRQHandler, Default_Handler

    .weak DMA1_Channel6_IRQHandler
    .thumb_set DMA1_Channel6_IRQHandler, Default_Handler

    .weak DMA1_Channel7_IRQHandler
    .thumb_set DMA1_Channel7_IRQHandler, Default_Handler

    .weak ADC1_2_IRQHandler
    .thumb_set ADC1_2_IRQHandler, Default_Handler

    .weak USB_HP_CAN1_TX_IRQHandler
    .thumb_set USB_HP_CAN1_TX_IRQHandler, Default_Handler

    .weak USB_LP_CAN1_RX0_IRQHandler
    .thumb_set USB_LP_CAN1_RX0_IRQHandler, Default_Handler

    .weak CAN1_RX1_IRQHandler
    .thumb_set CAN1_RX1_IRQHandler, Default_Handler

    .weak CAN1_SCE_IRQHandler
    .thumb_set CAN1_SCE_IRQHandler, Default_Handler

    .weak EXTI9_5_IRQHandler
    .thumb_set EXTI9_5_IRQHandler, Default_Handler

    .weak TIM1_BRK_IRQHandler
    .thumb_set TIM1_BRK_IRQHandler, Default_Handler

    .weak TIM1_UP_IRQHandler
    .thumb_set TIM1_UP_IRQHandler, Default_Handler

    .weak TIM1_TRG_COM_IRQHandler
    .thumb_set TIM1_TRG_COM_IRQHandler, Default_Handler

    .weak TIM1_CC_IRQHandler
    .thumb_set TIM1_CC_IRQHandler, Default_Handler

    .weak TIM2_IRQHandler
    .thumb_set TIM2_IRQHandler, Default_Handler

    .weak TIM3_IRQHandler
    .thumb_set TIM3_IRQHandler, Default_Handler

    .weak TIM4_IRQHandler
    .thumb_set TIM4_IRQHandler, Default_Handler

    .weak I2C1_EV_IRQHandler
    .thumb_set I2C1_EV_IRQHandler, Default_Handler

    .weak I2C1_ER_IRQHandler
    .thumb_set I2C1_ER_IRQHandler, Default_Handler

    .weak I2C2_EV_IRQHandler
    .thumb_set I2C2_EV_IRQHandler, Default_Handler

    .weak I2C2_ER_IRQHandler
    .thumb_set I2C2_ER_IRQHandler, Default_Handler

    .weak SPI1_IRQHandler
    .thumb_set SPI1_IRQHandler, Default_Handler

    .weak SPI2_IRQHandler
    .thumb_set SPI2_IRQHandler, Default_Handler

    .weak USART1_IRQHandler
    .thumb_set USART1_IRQHandler, Default_Handler

    .weak USART2_IRQHandler
    .thumb_set USART2_IRQHandler, Default_Handler

    .weak USART3_IRQHandler
    .thumb_set USART3_IRQHandler, Default_Handler

    .weak EXTI15_10_IRQHandler
    .thumb_set EXTI15_10_IRQHandler, Default_Handler

    .weak RTC_Alarm_IRQHandler
    .thumb_set RTC_Alarm_IRQHandler, Default_Handler

    .weak USBWakeUp_IRQHandler
    .thumb_set USBWakeUp_IRQHandler, Default_Handler

    .end
