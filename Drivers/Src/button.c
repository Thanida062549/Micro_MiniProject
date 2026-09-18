#include "button.h"
#include "timer.h"

/* =====================================================================
 * Button driver สำหรับ STM32F411RE
 * Pin mapping: ปุ่มตอบ 1-4 = PA10, PB3, PB5, PB4 (input, internal pull-up, active-low)
 * ===================================================================== */

#define RCC_BASE      0x40023800UL
#define GPIOA_BASE    0x40020000UL
#define GPIOB_BASE    0x40020400UL

#define RCC_AHB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x30))

#define GPIO_MODER(base)  (*(volatile uint32_t *)((base) + 0x00))
#define GPIO_PUPDR(base)  (*(volatile uint32_t *)((base) + 0x0C))
#define GPIO_IDR(base)    (*(volatile uint32_t *)((base) + 0x10))

typedef struct {
    uint32_t base;
    uint8_t  pin;
} PinDef_t;

static const PinDef_t BTN_PINS[4] = {
    { GPIOA_BASE, 10 },  /* ปุ่ม 1: PA10 */
    { GPIOB_BASE, 3  },  /* ปุ่ม 2: PB3  */
    { GPIOB_BASE, 5  },  /* ปุ่ม 3: PB5  */
    { GPIOB_BASE, 4  },  /* ปุ่ม 4: PB4  */
};

void Button_Init(void)
{
    RCC_AHB1ENR |= (1UL << 0) | (1UL << 1); /* GPIOA, GPIOB */

    for (uint8_t i = 0; i < 4; i++) {
        uint32_t base = BTN_PINS[i].base;
        uint8_t  pin  = BTN_PINS[i].pin;

        GPIO_MODER(base) &= ~(3UL << (pin * 2));       /* 00 = input (ค่า default อยู่แล้ว) */
        GPIO_PUPDR(base) &= ~(3UL << (pin * 2));
        GPIO_PUPDR(base) |=  (1UL << (pin * 2));        /* 01 = pull-up */
    }
}

uint8_t BSP_Button_Read(void)
{
    for (uint8_t i = 0; i < 4; i++) {
        if ((GPIO_IDR(BTN_PINS[i].base) & (1UL << BTN_PINS[i].pin)) == 0) { /* active-low */
            return (uint8_t)(i + 1);
        }
    }
    return 0;
}

uint8_t BSP_Button_WaitPress(void)
{
    const uint8_t STABLE_COUNT = 3;
    uint8_t last = 0;
    uint8_t stable = 0;

    while (BSP_Button_Read() != 0) {
        BSP_Delay_ms(5); /* รอให้ปุ่มถูกปล่อยก่อน กันค้างจากการกดครั้งก่อน */
    }

    for (;;) {
        uint8_t current = BSP_Button_Read();
        if (current != 0 && current == last) {
            stable++;
            if (stable >= STABLE_COUNT) {
                return current;
            }
        } else {
            stable = 0;
        }
        last = current;
        BSP_Delay_ms(5);
    }
}
