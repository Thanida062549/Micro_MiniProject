#include "led.h"

/* =====================================================================
 * LED driver สำหรับ STM32F411RE
 * Pin mapping: LED 1-4 = PA5, PA6, PA7, PB6 (output push-pull, active-high)
 * ===================================================================== */

#define RCC_BASE      0x40023800UL
#define GPIOA_BASE    0x40020000UL
#define GPIOB_BASE    0x40020400UL

#define RCC_AHB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x30))

#define GPIO_MODER(base)    (*(volatile uint32_t *)((base) + 0x00))
#define GPIO_OTYPER(base)   (*(volatile uint32_t *)((base) + 0x04))
#define GPIO_BSRR(base)     (*(volatile uint32_t *)((base) + 0x18))

typedef struct {
    uint32_t base;
    uint8_t  pin;
} PinDef_t;

static const PinDef_t LED_PINS[4] = {
    { GPIOA_BASE, 5 },   /* LED 1: PA5 */
    { GPIOA_BASE, 6 },   /* LED 2: PA6 */
    { GPIOA_BASE, 7 },   /* LED 3: PA7 */
    { GPIOB_BASE, 6 },   /* LED 4: PB6 */
};

void LED_Init(void)
{
    RCC_AHB1ENR |= (1UL << 0) | (1UL << 1); /* GPIOA, GPIOB */

    for (uint8_t i = 0; i < 4; i++) {
        uint32_t base = LED_PINS[i].base;
        uint8_t  pin  = LED_PINS[i].pin;

        GPIO_MODER(base)  &= ~(3UL << (pin * 2));
        GPIO_MODER(base)  |=  (1UL << (pin * 2));  /* 01 = output */
        GPIO_OTYPER(base) &= ~(1UL << pin);        /* 0 = push-pull */
        GPIO_BSRR(base)    = (1UL << (pin + 16));  /* ปิดไฟตั้งแต่เริ่ม */
    }
}

void BSP_LED_On(uint8_t led_index)
{
    if (led_index >= 1 && led_index <= 4) {
        const PinDef_t *p = &LED_PINS[led_index - 1];
        GPIO_BSRR(p->base) = (1UL << p->pin);
    }
}

void BSP_LED_Off(uint8_t led_index)
{
    if (led_index >= 1 && led_index <= 4) {
        const PinDef_t *p = &LED_PINS[led_index - 1];
        GPIO_BSRR(p->base) = (1UL << (p->pin + 16));
    }
}

void BSP_LED_AllOff(void)
{
    for (uint8_t i = 1; i <= 4; i++) {
        BSP_LED_Off(i);
    }
}
