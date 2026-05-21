/**
 * @file    startup_stm32f407xx.s
 * @brief   STM32F407xx Cortex-M4 startup file (GNU ARM toolchain).
 *
 * This startup code:
 *   1. Sets the initial stack pointer
 *   2. Defines the vector table with all STM32F4 interrupt handlers
 *   3. Provides the Reset_Handler that copies .data from Flash to RAM,
 *      zeros .bss, calls SystemInit(), and then calls main()
 *   4. Provides weak default definitions for all interrupt handlers
 *
 * Adapted from STM32CubeF4 CMSIS startup template.
 */

    .syntax unified
    .cpu    cortex-m4
    .fpu    softvfp
    .thumb

/* --------------------------------------------------------------------------
 * Global symbols (exported)
 * -------------------------------------------------------------------------- */
    .global g_pfnVectors
    .global Default_Handler

    /* Linker-script-provided symbols */
    .word _sidata   /* Start of .data initializers in Flash */
    .word _sdata    /* Start of .data section in RAM */
    .word _edata    /* End of .data section in RAM */
    .word _sbss     /* Start of .bss section */
    .word _ebss     /* End of .bss section */

/* --------------------------------------------------------------------------
 * External references
 * -------------------------------------------------------------------------- */
    .extern main               /* User main() */
    .extern SystemInit         /* CMSIS system initialization */

/* --------------------------------------------------------------------------
 * Section: Reset handler and vector table (placed in .isr_vector)
 * -------------------------------------------------------------------------- */
    .section .isr_vector, "a", %progbits
    .type   g_pfnVectors, %object
    .size   g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .word _estack                                  /* 0  - Initial Stack Pointer */
    .word Reset_Handler                            /* 1  - Reset */
    .word NMI_Handler                              /* 2  - NMI */
    .word HardFault_Handler                        /* 3  - HardFault */
    .word MemManage_Handler                        /* 4  - MPU Fault */
    .word BusFault_Handler                         /* 5  - Bus Fault */
    .word UsageFault_Handler                       /* 6  - Usage Fault */
    .word 0                                        /* 7  - Reserved */
    .word 0                                        /* 8  - Reserved */
    .word 0                                        /* 9  - Reserved */
    .word 0                                        /* 10 - Reserved */
    .word SVC_Handler                              /* 11 - SVCall */
    .word DebugMon_Handler                         /* 12 - Debug Monitor */
    .word 0                                        /* 13 - Reserved */
    .word PendSV_Handler                           /* 14 - PendSV */
    .word SysTick_Handler                          /* 15 - SysTick */

    /* External Interrupts (STM32F407xx specific, 82+16=98 total vectors) */
    .word WWDG_IRQHandler                          /* 16 - Window Watchdog */
    .word PVD_IRQHandler                           /* 17 - PVD through EXTI */
    .word TAMP_STAMP_IRQHandler                    /* 18 - Tamper / TimeStamp */
    .word RTC_WKUP_IRQHandler                      /* 19 - RTC Wakeup */
    .word FLASH_IRQHandler                         /* 20 - Flash */
    .word RCC_IRQHandler                           /* 21 - RCC */
    .word EXTI0_IRQHandler                         /* 22 - EXTI Line 0 */
    .word EXTI1_IRQHandler                         /* 23 - EXTI Line 1 */
    .word EXTI2_IRQHandler                         /* 24 - EXTI Line 2 */
    .word EXTI3_IRQHandler                         /* 25 - EXTI Line 3 */
    .word EXTI4_IRQHandler                         /* 26 - EXTI Line 4 */
    .word DMA1_Stream0_IRQHandler                  /* 27 - DMA1 Stream 0 */
    .word DMA1_Stream1_IRQHandler                  /* 28 - DMA1 Stream 1 */
    .word DMA1_Stream2_IRQHandler                  /* 29 - DMA1 Stream 2 */
    .word DMA1_Stream3_IRQHandler                  /* 30 - DMA1 Stream 3 */
    .word DMA1_Stream4_IRQHandler                  /* 31 - DMA1 Stream 4 */
    .word DMA1_Stream5_IRQHandler                  /* 32 - DMA1 Stream 5 */
    .word DMA1_Stream6_IRQHandler                  /* 33 - DMA1 Stream 6 */
    .word ADC_IRQHandler                           /* 34 - ADC1/2/3 */
    .word CAN1_TX_IRQHandler                       /* 35 - CAN1 TX */
    .word CAN1_RX0_IRQHandler                      /* 36 - CAN1 RX0 */
    .word CAN1_RX1_IRQHandler                      /* 37 - CAN1 RX1 */
    .word CAN1_SCE_IRQHandler                      /* 38 - CAN1 SCE */
    .word EXTI9_5_IRQHandler                       /* 39 - EXTI Line 9-5 */
    .word TIM1_BRK_TIM9_IRQHandler                 /* 40 - TIM1 Break / TIM9 */
    .word TIM1_UP_TIM10_IRQHandler                 /* 41 - TIM1 Update / TIM10 */
    .word TIM1_TRG_COM_TIM11_IRQHandler            /* 42 - TIM1 Trg/Com / TIM11 */
    .word TIM1_CC_IRQHandler                       /* 43 - TIM1 Capture Compare */
    .word TIM2_IRQHandler                          /* 44 - TIM2 */
    .word TIM3_IRQHandler                          /* 45 - TIM3 */
    .word TIM4_IRQHandler                          /* 46 - TIM4 */
    .word I2C1_EV_IRQHandler                       /* 47 - I2C1 Event */
    .word I2C1_ER_IRQHandler                       /* 48 - I2C1 Error */
    .word I2C2_EV_IRQHandler                       /* 49 - I2C2 Event */
    .word I2C2_ER_IRQHandler                       /* 50 - I2C2 Error */
    .word SPI1_IRQHandler                          /* 51 - SPI1 */
    .word SPI2_IRQHandler                          /* 52 - SPI2 */
    .word USART1_IRQHandler                        /* 53 - USART1 */
    .word USART2_IRQHandler                        /* 54 - USART2 */
    .word USART3_IRQHandler                        /* 55 - USART3 */
    .word EXTI15_10_IRQHandler                     /* 56 - EXTI Line 15-10 */
    .word RTC_Alarm_IRQHandler                     /* 57 - RTC Alarm */
    .word OTG_FS_WKUP_IRQHandler                   /* 58 - USB OTG FS Wakeup */
    .word TIM8_BRK_TIM12_IRQHandler                /* 59 - TIM8 Break / TIM12 */
    .word TIM8_UP_TIM13_IRQHandler                 /* 60 - TIM8 Update / TIM13 */
    .word TIM8_TRG_COM_TIM14_IRQHandler            /* 61 - TIM8 Trg/Com / TIM14 */
    .word TIM8_CC_IRQHandler                       /* 62 - TIM8 Capture Compare */
    .word DMA1_Stream7_IRQHandler                  /* 63 - DMA1 Stream 7 */
    .word FSMC_IRQHandler                          /* 64 - FSMC */
    .word SDIO_IRQHandler                          /* 65 - SDIO */
    .word TIM5_IRQHandler                          /* 66 - TIM5 */
    .word SPI3_IRQHandler                          /* 67 - SPI3 */
    .word UART4_IRQHandler                         /* 68 - UART4 */
    .word UART5_IRQHandler                         /* 69 - UART5 */
    .word TIM6_DAC_IRQHandler                      /* 70 - TIM6 + DAC */
    .word TIM7_IRQHandler                          /* 71 - TIM7 */
    .word DMA2_Stream0_IRQHandler                  /* 72 - DMA2 Stream 0 */
    .word DMA2_Stream1_IRQHandler                  /* 73 - DMA2 Stream 1 */
    .word DMA2_Stream2_IRQHandler                  /* 74 - DMA2 Stream 2 */
    .word DMA2_Stream3_IRQHandler                  /* 75 - DMA2 Stream 3 */
    .word DMA2_Stream4_IRQHandler                  /* 76 - DMA2 Stream 4 */
    .word ETH_IRQHandler                           /* 77 - Ethernet */
    .word ETH_WKUP_IRQHandler                      /* 78 - Ethernet Wakeup */
    .word CAN2_TX_IRQHandler                       /* 79 - CAN2 TX */
    .word CAN2_RX0_IRQHandler                      /* 80 - CAN2 RX0 */
    .word CAN2_RX1_IRQHandler                      /* 81 - CAN2 RX1 */
    .word CAN2_SCE_IRQHandler                      /* 82 - CAN2 SCE */
    .word OTG_FS_IRQHandler                        /* 83 - USB OTG FS */
    .word DMA2_Stream5_IRQHandler                  /* 84 - DMA2 Stream 5 */
    .word DMA2_Stream6_IRQHandler                  /* 85 - DMA2 Stream 6 */
    .word DMA2_Stream7_IRQHandler                  /* 86 - DMA2 Stream 7 */
    .word USART6_IRQHandler                        /* 87 - USART6 */
    .word I2C3_EV_IRQHandler                       /* 88 - I2C3 Event */
    .word I2C3_ER_IRQHandler                       /* 89 - I2C3 Error */
    .word OTG_HS_EP1_OUT_IRQHandler                /* 90 - USB OTG HS EP1 Out */
    .word OTG_HS_EP1_IN_IRQHandler                 /* 91 - USB OTG HS EP1 In */
    .word OTG_HS_WKUP_IRQHandler                   /* 92 - USB OTG HS Wakeup */
    .word OTG_HS_IRQHandler                        /* 93 - USB OTG HS */
    .word DCMI_IRQHandler                          /* 94 - DCMI */
    .word 0                                        /* 95 - Reserved */
    .word HASH_RNG_IRQHandler                      /* 96 - HASH / RNG */
    .word FPU_IRQHandler                           /* 97 - FPU */

/* --------------------------------------------------------------------------
 * Reset Handler
 * -------------------------------------------------------------------------- */
    .section .text.Reset_Handler, "ax", %progbits
    .type   Reset_Handler, %function
    .weak   Reset_Handler

Reset_Handler:
    /* Initialize the FPU if present (Cortex-M4) */
    ldr   r0, =0xE000ED88      /* CPACR register address */
    ldr   r1, [r0]
    orr   r1, r1, #(0xF << 20) /* Enable CP10 and CP11 (FPU) */
    str   r1, [r0]
    dsb
    isb

    /* Copy .data section initializers from Flash to RAM */
    ldr   r0, =_sdata          /* Destination: start of .data in RAM */
    ldr   r1, =_edata          /* Destination: end of .data in RAM */
    ldr   r2, =_sidata         /* Source: start of .data in Flash */
    b     LoopCopyDataInit

CopyDataInit:
    ldr   r4, [r2], #4        /* Load word from Flash, increment source */
    str   r4, [r0], #4        /* Store word to RAM, increment dest */
LoopCopyDataInit:
    cmp   r0, r1
    bcc   CopyDataInit        /* Loop while dest < end */

    /* Zero-fill .bss section */
    ldr   r0, =_sbss
    ldr   r1, =_ebss
    movs  r2, #0
    b     LoopFillZerobss

FillZerobss:
    str   r2, [r0], #4
LoopFillZerobss:
    cmp   r0, r1
    bcc   FillZerobss

    /* Call CMSIS SystemInit() to set up clocks */
    bl    SystemInit

    /* Call static constructors */
    bl    __libc_init_array

    /* Jump to main() */
    bl    main

    /* Fall into infinite loop if main returns */
    b     .

    .size Reset_Handler, .-Reset_Handler

/* --------------------------------------------------------------------------
 * Default Handler (weak) - Infinite loop for unexpected interrupts
 * -------------------------------------------------------------------------- */
    .section .text.Default_Handler, "ax", %progbits
    .type   Default_Handler, %function
    .weak   Default_Handler

Default_Handler:
Infinite_Loop:
    b     Infinite_Loop
    .size Default_Handler, .-Default_Handler

/* --------------------------------------------------------------------------
 * Weak Aliases for Interrupt Handlers
 * All point to Default_Handler unless overridden by user code.
 * -------------------------------------------------------------------------- */
    .macro  def_irq_handler  handler_name
    .weak   \handler_name
    .thumb_set \handler_name, Default_Handler
    .endm

    def_irq_handler NMI_Handler
    def_irq_handler HardFault_Handler
    def_irq_handler MemManage_Handler
    def_irq_handler BusFault_Handler
    def_irq_handler UsageFault_Handler
    def_irq_handler SVC_Handler
    def_irq_handler DebugMon_Handler
    def_irq_handler PendSV_Handler
    def_irq_handler SysTick_Handler
    def_irq_handler WWDG_IRQHandler
    def_irq_handler PVD_IRQHandler
    def_irq_handler TAMP_STAMP_IRQHandler
    def_irq_handler RTC_WKUP_IRQHandler
    def_irq_handler FLASH_IRQHandler
    def_irq_handler RCC_IRQHandler
    def_irq_handler EXTI0_IRQHandler
    def_irq_handler EXTI1_IRQHandler
    def_irq_handler EXTI2_IRQHandler
    def_irq_handler EXTI3_IRQHandler
    def_irq_handler EXTI4_IRQHandler
    def_irq_handler DMA1_Stream0_IRQHandler
    def_irq_handler DMA1_Stream1_IRQHandler
    def_irq_handler DMA1_Stream2_IRQHandler
    def_irq_handler DMA1_Stream3_IRQHandler
    def_irq_handler DMA1_Stream4_IRQHandler
    def_irq_handler DMA1_Stream5_IRQHandler
    def_irq_handler DMA1_Stream6_IRQHandler
    def_irq_handler ADC_IRQHandler
    def_irq_handler CAN1_TX_IRQHandler
    def_irq_handler CAN1_RX0_IRQHandler
    def_irq_handler CAN1_RX1_IRQHandler
    def_irq_handler CAN1_SCE_IRQHandler
    def_irq_handler EXTI9_5_IRQHandler
    def_irq_handler TIM1_BRK_TIM9_IRQHandler
    def_irq_handler TIM1_UP_TIM10_IRQHandler
    def_irq_handler TIM1_TRG_COM_TIM11_IRQHandler
    def_irq_handler TIM1_CC_IRQHandler
    def_irq_handler TIM2_IRQHandler
    def_irq_handler TIM3_IRQHandler
    def_irq_handler TIM4_IRQHandler
    def_irq_handler I2C1_EV_IRQHandler
    def_irq_handler I2C1_ER_IRQHandler
    def_irq_handler I2C2_EV_IRQHandler
    def_irq_handler I2C2_ER_IRQHandler
    def_irq_handler SPI1_IRQHandler
    def_irq_handler SPI2_IRQHandler
    def_irq_handler USART1_IRQHandler
    def_irq_handler USART2_IRQHandler
    def_irq_handler USART3_IRQHandler
    def_irq_handler EXTI15_10_IRQHandler
    def_irq_handler RTC_Alarm_IRQHandler
    def_irq_handler OTG_FS_WKUP_IRQHandler
    def_irq_handler TIM8_BRK_TIM12_IRQHandler
    def_irq_handler TIM8_UP_TIM13_IRQHandler
    def_irq_handler TIM8_TRG_COM_TIM14_IRQHandler
    def_irq_handler TIM8_CC_IRQHandler
    def_irq_handler DMA1_Stream7_IRQHandler
    def_irq_handler FSMC_IRQHandler
    def_irq_handler SDIO_IRQHandler
    def_irq_handler TIM5_IRQHandler
    def_irq_handler SPI3_IRQHandler
    def_irq_handler UART4_IRQHandler
    def_irq_handler UART5_IRQHandler
    def_irq_handler TIM6_DAC_IRQHandler
    def_irq_handler TIM7_IRQHandler
    def_irq_handler DMA2_Stream0_IRQHandler
    def_irq_handler DMA2_Stream1_IRQHandler
    def_irq_handler DMA2_Stream2_IRQHandler
    def_irq_handler DMA2_Stream3_IRQHandler
    def_irq_handler DMA2_Stream4_IRQHandler
    def_irq_handler ETH_IRQHandler
    def_irq_handler ETH_WKUP_IRQHandler
    def_irq_handler CAN2_TX_IRQHandler
    def_irq_handler CAN2_RX0_IRQHandler
    def_irq_handler CAN2_RX1_IRQHandler
    def_irq_handler CAN2_SCE_IRQHandler
    def_irq_handler OTG_FS_IRQHandler
    def_irq_handler DMA2_Stream5_IRQHandler
    def_irq_handler DMA2_Stream6_IRQHandler
    def_irq_handler DMA2_Stream7_IRQHandler
    def_irq_handler USART6_IRQHandler
    def_irq_handler I2C3_EV_IRQHandler
    def_irq_handler I2C3_ER_IRQHandler
    def_irq_handler OTG_HS_EP1_OUT_IRQHandler
    def_irq_handler OTG_HS_EP1_IN_IRQHandler
    def_irq_handler OTG_HS_WKUP_IRQHandler
    def_irq_handler OTG_HS_IRQHandler
    def_irq_handler DCMI_IRQHandler
    def_irq_handler HASH_RNG_IRQHandler
    def_irq_handler FPU_IRQHandler

    .end
