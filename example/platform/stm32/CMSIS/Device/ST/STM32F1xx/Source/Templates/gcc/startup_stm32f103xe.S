/* Minimal GCC startup derived from ST template (Keil asm exists). */

.syntax unified
.cpu cortex-m3
.fpu softvfp
.thumb

.global  g_pfnVectors
.global  Default_Handler

/* Symbols provided by linker script */
.extern _estack
.extern _sidata
.extern _sdata
.extern _edata
.extern _sbss
.extern _ebss

/* Vector table */
.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
.size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
  .word  _estack
  .word  Reset_Handler
  .word  NMI_Handler
  .word  HardFault_Handler
  .word  MemManage_Handler
  .word  BusFault_Handler
  .word  UsageFault_Handler
  .word  0
  .word  0
  .word  0
  .word  0
  .word  SVC_Handler
  .word  DebugMon_Handler
  .word  0
  .word  PendSV_Handler
  .word  SysTick_Handler
  /* External interrupts (STM32F103xE count, names must match stm32f1xx_it.c) */
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

/* Reset handler */
.text
.thumb
.global Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  /* Initialize system clock and low-level */
  bl  SystemInit

  /* Copy data section from FLASH to RAM */
  ldr r0, =_sidata
  ldr r1, =_sdata
  ldr r2, =_edata
1:
  cmp r1, r2
  ittt lt
  ldrlt r3, [r0], #4
  strlt r3, [r1], #4
  blt 1b

  /* Zero initialize .bss */
  ldr r0, =_sbss
  ldr r1, =_ebss
  movs r2, #0
2:
  cmp r0, r1
  itt lt
  strlt r2, [r0], #4
  blt 2b

  /* Run static constructors */
  bl  __libc_init_array

  /* Call main */
  bl  main
1: b 1b

/* Weak aliases for exception handlers to Default_Handler */
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
.weak RTC_Alarm_IRQHandler
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

Default_Handler:
  b .
