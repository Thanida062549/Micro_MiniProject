#include "buzzer.h"
#include "timer.h"

/* =====================================================================
 * Buzzer driver สำหรับ STM32F411RE
 * Pin mapping: Buzzer = PC0 (output push-pull)
 * หมายเหตุ: PA8 ห้ามใช้ เพราะชิลด์ STEO ผูกไว้กับวงจร 7-segment แล้ว
 * ===================================================================== */

#define RCC_BASE      0x40023800UL
#define GPIOC_BASE    0x40020800UL

#define RCC_AHB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x30))

#define GPIO_MODER(base)    (*(volatile uint32_t *)((base) + 0x00))
#define GPIO_OTYPER(base)   (*(volatile uint32_t *)((base) + 0x04))
#define GPIO_BSRR(base)     (*(volatile uint32_t *)((base) + 0x18))

#define BUZZER_BASE  GPIOC_BASE
#define BUZZER_PIN   0

void Buzzer_Init(void)
{
    RCC_AHB1ENR |= (1UL << 2); /* GPIOC */

    GPIO_MODER(BUZZER_BASE)  &= ~(3UL << (BUZZER_PIN * 2));
    GPIO_MODER(BUZZER_BASE)  |=  (1UL << (BUZZER_PIN * 2)); /* 01 = output */
    GPIO_OTYPER(BUZZER_BASE) &= ~(1UL << BUZZER_PIN);       /* 0 = push-pull */
    GPIO_BSRR(BUZZER_BASE)    = (1UL << (BUZZER_PIN + 16)); /* ปิดตั้งแต่เริ่ม */
}

void BSP_Buzzer_Beep(uint16_t duration_ms)
{
    GPIO_BSRR(BUZZER_BASE) = (1UL << BUZZER_PIN);
    BSP_Delay_ms(duration_ms);
    GPIO_BSRR(BUZZER_BASE) = (1UL << (BUZZER_PIN + 16));
}
