#include "buzzer.h"
#include "timer.h"
#include <stdint.h>

#define GPIOC_BASE      0x40020800UL
#define RCC_BASE        0x40023800UL

#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))

#define GPIO_MODER      (*(volatile uint32_t *)(GPIOC_BASE + 0x00))
#define GPIO_OTYPER     (*(volatile uint32_t *)(GPIOC_BASE + 0x04))
#define GPIO_PUPDR      (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))
#define GPIO_BSRR       (*(volatile uint32_t *)(GPIOC_BASE + 0x18))

#define BUZZER_PIN      2

/* PC2 = HIGH / LOW เพื่อสร้างเสียงให้ Passive Buzzer */

/*
 * STM32F411 = 16 MHz
 *
 * เสียง 2 kHz
 * 1 รอบ = 500 us
 * ครึ่งรอบ = 250 us
 * 16 MHz x 250 us = 2000 cycles
 */

#define BUZZER_HALF_PERIOD_CYCLES  4000UL


/* -------------------------------------------------------
 * DWT Cycle Counter
 * ใช้จับเวลาระดับ microsecond โดยไม่ต้องเพิ่ม Timer ตัวใหม่
 * ------------------------------------------------------- */

#define DEMCR       (*(volatile uint32_t *)0xE000EDFCUL)
#define DWT_CTRL    (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004UL)


static void Buzzer_DWT_Init(void)
{
    /* เปิด DWT */
    DEMCR |= (1UL << 24);

    /* เปิด Cycle Counter */
    DWT_CYCCNT = 0;
    DWT_CTRL |= 1UL;
}


static void Buzzer_WaitCycles(uint32_t cycles)
{
    uint32_t start = DWT_CYCCNT;

    while ((uint32_t)(DWT_CYCCNT - start) < cycles)
    {
        /* wait */
    }
}


/* -------------------------------------------------------
 * Buzzer_Init
 * ------------------------------------------------------- */

void Buzzer_Init(void)
{
    /* เปิด GPIOC clock */
    RCC_AHB1ENR |= (1UL << 2);

    /* PC2 = General Purpose Output */
    GPIO_MODER &= ~(3UL << (BUZZER_PIN * 2));
    GPIO_MODER |=  (1UL << (BUZZER_PIN * 2));

    /* Push-pull */
    GPIO_OTYPER &= ~(1UL << BUZZER_PIN);

    /* No pull-up / pull-down */
    GPIO_PUPDR &= ~(3UL << (BUZZER_PIN * 2));

    /* เริ่มต้น = LOW → buzzer เงียบ */
    GPIO_BSRR = (1UL << (BUZZER_PIN + 16));

    /* เปิด DWT */
    Buzzer_DWT_Init();
}


/* -------------------------------------------------------
 * BSP_Buzzer_Beep
 *
 * Passive buzzer:
 * สร้าง square wave 2 kHz เป็นเวลา duration_ms
 * ------------------------------------------------------- */

void BSP_Buzzer_Beep(uint16_t duration_ms)
{
    uint32_t start_ms = Timer_GetTick();

    while ((uint32_t)(Timer_GetTick() - start_ms) < duration_ms)
    {
        /* HIGH */
        GPIO_BSRR = (1UL << BUZZER_PIN);

        Buzzer_WaitCycles(BUZZER_HALF_PERIOD_CYCLES);

        /* LOW */
        GPIO_BSRR = (1UL << (BUZZER_PIN + 16));

        Buzzer_WaitCycles(BUZZER_HALF_PERIOD_CYCLES);
    }

    /* จบเสียง → บังคับ LOW */
    GPIO_BSRR = (1UL << (BUZZER_PIN + 16));
}

