#include "timer.h"

/* =====================================================================
 * Timer driver (SysTick) สำหรับ STM32F411RE
 * ความถี่ระบบสมมติ HSI default 16 MHz — ถ้าตั้ง PLL ไว้ต่างจากนี้ ต้องแก้ SYSTEM_CLOCK_HZ
 * ===================================================================== */

#define SYSTEM_CLOCK_HZ   16000000UL

#define SYSTICK_CTRL   (*(volatile uint32_t *)0xE000E010UL)
#define SYSTICK_LOAD   (*(volatile uint32_t *)0xE000E014UL)
#define SYSTICK_VAL    (*(volatile uint32_t *)0xE000E018UL)

static volatile uint32_t ms_ticks = 0;

void Timer_Init(void)
{
    SYSTICK_LOAD = (SYSTEM_CLOCK_HZ / 1000UL) - 1UL;
    SYSTICK_VAL  = 0;
    SYSTICK_CTRL = (1UL << 0) | (1UL << 1) | (1UL << 2); /* ENABLE, TICKINT, CLKSOURCE=processor */
}

/* ชื่อฟังก์ชันต้องตรงกับ vector table ใน startup_stm32f411retx.s */
void SysTick_Handler(void)
{
    ms_ticks++;
}

void BSP_Delay_ms(uint32_t ms)
{
    uint32_t start = ms_ticks;
    while ((uint32_t)(ms_ticks - start) < ms) {
        /* รอ ms_ticks ที่นับโดย SysTick interrupt */
    }
}

uint32_t Timer_GetTick(void)
{
    return ms_ticks;
}
