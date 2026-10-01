#include "timer.h"
#include <stdint.h>

/* =====================================================================
 * Timer driver สำหรับ STM32F411RE
 *
 * ใช้ TIM3 Hardware Timer แทน SysTick
 *
 * System Clock = 16 MHz
 * TIM3 Clock   = 16 MHz
 * Prescaler    = 16 MHz / 1 MHz - 1 = 15
 * Auto Reload  = 1 MHz / 1000 - 1 = 999
 *
 * ดังนั้น TIM3 Update Interrupt เกิดทุก 1 ms
 * ===================================================================== */

#define SYSTEM_CLOCK_HZ   16000000UL

/* =========================
 * RCC
 * ========================= */
#define RCC_BASE          0x40023800UL

#define RCC_APB1ENR       (*(volatile uint32_t *)(RCC_BASE + 0x40))

/* TIM3 clock enable = APB1ENR bit 1 */
#define RCC_APB1ENR_TIM3EN   (1UL << 1)

/* =========================
 * TIM3 Registers
 * ========================= */
#define TIM3_BASE         0x40000400UL

#define TIM3_CR1          (*(volatile uint32_t *)(TIM3_BASE + 0x00))
#define TIM3_DIER         (*(volatile uint32_t *)(TIM3_BASE + 0x0C))
#define TIM3_SR           (*(volatile uint32_t *)(TIM3_BASE + 0x10))
#define TIM3_EGR          (*(volatile uint32_t *)(TIM3_BASE + 0x14))
#define TIM3_PSC          (*(volatile uint32_t *)(TIM3_BASE + 0x28))
#define TIM3_ARR          (*(volatile uint32_t *)(TIM3_BASE + 0x2C))

/* =========================
 * TIM3 Bit Definitions
 * ========================= */

/* CR1 */
#define TIM_CR1_CEN       (1UL << 0)

/* DIER */
#define TIM_DIER_UIE      (1UL << 0)

/* SR */
#define TIM_SR_UIF        (1UL << 0)

/* EGR */
#define TIM_EGR_UG        (1UL << 0)

/* =========================
 * NVIC
 * ========================= */

/*
 * TIM3 IRQ number = 29
 * IRQ 0-31 อยู่ใน NVIC_ISER0
 */
#define NVIC_ISER0        (*(volatile uint32_t *)0xE000E100UL)
#define TIM3_IRQn         29

/* =========================
 * Millisecond counter
 * ========================= */

static volatile uint32_t ms_ticks = 0UL;


/* =====================================================================
 * Timer_Init
 *
 * ตั้งค่า TIM3 ให้เกิด Interrupt ทุก 1 ms
 * ===================================================================== */
void Timer_Init(void)
{
    /*
     * เปิด Clock ให้ TIM3
     */
    RCC_APB1ENR |= RCC_APB1ENR_TIM3EN;

    /*
     * หยุด TIM3 ก่อนตั้งค่า
     */
    TIM3_CR1 &= ~TIM_CR1_CEN;

    /*
     * Prescaler
     *
     * 16 MHz / (15 + 1)
     * = 1 MHz
     *
     * 1 tick = 1 microsecond
     */
    TIM3_PSC = 15UL;

    /*
     * Auto Reload
     *
     * 1 MHz / 1000
     * = 1000 Hz
     *
     * Interrupt ทุก 1 ms
     */
    TIM3_ARR = 999UL;

    /*
     * Reset counter
     */
    TIM3_EGR = TIM_EGR_UG;

    /*
     * Clear Update Interrupt Flag
     */
    TIM3_SR &= ~TIM_SR_UIF;

    /*
     * Enable Update Interrupt
     */
    TIM3_DIER |= TIM_DIER_UIE;

    /*
     * Enable TIM3 interrupt ใน NVIC
     */
    NVIC_ISER0 = (1UL << TIM3_IRQn);

    /*
     * Reset software millisecond counter
     */
    ms_ticks = 0UL;

    /*
     * Start TIM3
     */
    TIM3_CR1 |= TIM_CR1_CEN;
}


/* =====================================================================
 * TIM3_IRQHandler
 *
 * ถูกเรียกทุก ๆ 1 ms
 * ===================================================================== */
void TIM3_IRQHandler(void)
{
    if ((TIM3_SR & TIM_SR_UIF) != 0UL)
    {
        /*
         * Clear Update Interrupt Flag
         */
        TIM3_SR &= ~TIM_SR_UIF;

        /*
         * เพิ่มเวลาทีละ 1 ms
         */
        ms_ticks++;
    }
}


/* =====================================================================
 * BSP_Delay_ms
 *
 * Interface เดิม
 *
 * ไม่ต้องแก้ buzzer.c หรือไฟล์อื่น
 * ===================================================================== */
void BSP_Delay_ms(uint32_t ms)
{
    uint32_t start;

    start = ms_ticks;

    while ((uint32_t)(ms_ticks - start) < ms)
    {
        /*
         * รอให้ TIM3 Interrupt เพิ่ม ms_ticks
         */
    }
}


/* =====================================================================
 * Timer_GetTick
 *
 * Interface เดิม
 * ===================================================================== */
uint32_t Timer_GetTick(void)
{
    return ms_ticks;
}
