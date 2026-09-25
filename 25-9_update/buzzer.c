#include "buzzer.h"
#include "timer.h"
#include <stdint.h>

#define GPIOC_BASE      0x40020800UL
#define RCC_BASE        0x40023800UL

#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))

#define GPIO_MODER      (*(volatile uint32_t *)(GPIOC_BASE + 0x00))
#define GPIO_OTYPER     (*(volatile uint32_t *)(GPIOC_BASE + 0x04))
#define GPIO_OSPEEDR    (*(volatile uint32_t *)(GPIOC_BASE + 0x08))
#define GPIO_PUPDR      (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))
#define GPIO_BSRR       (*(volatile uint32_t *)(GPIOC_BASE + 0x18))

#define BUZZER_PIN      2


void Buzzer_Init(void)
{
    /* เปิด Clock ให้ GPIOC */
    RCC_AHB1ENR |= (1UL << 2);

    /* PC2 = Output */
    GPIO_MODER &= ~(3UL << (BUZZER_PIN * 2));
    GPIO_MODER |=  (1UL << (BUZZER_PIN * 2));

    /* Push-Pull */
    GPIO_OTYPER &= ~(1UL << BUZZER_PIN);

    /* High Speed */
    GPIO_OSPEEDR &= ~(3UL << (BUZZER_PIN * 2));
    GPIO_OSPEEDR |=  (3UL << (BUZZER_PIN * 2));

    /* No Pull-up / Pull-down */
    GPIO_PUPDR &= ~(3UL << (BUZZER_PIN * 2));

    /*
     * เริ่มเกม = ปิดเสียง
     *
     * การต่อ:
     * HW-512 VCC/+ -> PC2
     * HW-512 I/O   -> 3.3V
     * HW-512 GND/- -> GND
     *
     * LOW  = ปิดเสียง
     * HIGH = เปิดเสียง
     */
    GPIO_BSRR = (1UL << (BUZZER_PIN + 16));
}


void Buzzer_Stop(void)
{
    /* LOW = ปิดเสียง */
    GPIO_BSRR = (1UL << (BUZZER_PIN + 16));
}


void BSP_Buzzer_Beep(uint16_t duration_ms)
{
    /* HIGH = เปิดเสียง */
    GPIO_BSRR = (1UL << BUZZER_PIN);

    /*
     * ดังตามเวลาที่กำหนด
     */
    BSP_Delay_ms(duration_ms);

    /*
     * ครบเวลาแล้วปิดทันที
     */
    GPIO_BSRR = (1UL << (BUZZER_PIN + 16));
}
