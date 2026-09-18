#include "exti.h"
#include <stdint.h>

/* =====================================================================
 * EXTI driver สำหรับ STM32F411RE — ปุ่ม Reset
 * Pin mapping: PC13 (User Button B1 บนบอร์ด Nucleo), rising edge trigger
 *
 * แทนที่จะตั้ง flag ให้เกมไปเช็คทีละจุด (ซึ่งต้องแทรกโค้ดเข้าไปในทุกจังหวะ
 * ที่บล็อกรอของ Game Logic) เราเลือกสั่งรีสตาร์ตทั้งชิปทันทีผ่าน Cortex-M
 * System Reset — ผลลัพธ์เหมือนกดปุ่ม NRST จริง คือกลับไปเริ่ม main() ใหม่
 * ทั้งหมด โดยไม่ต้องแก้ไฟล์ Game Logic แม้แต่บรรทัดเดียว
 * ===================================================================== */

#define RCC_BASE       0x40023800UL
#define GPIOC_BASE     0x40020800UL
#define EXTI_BASE      0x40013C00UL
#define SYSCFG_BASE    0x40013800UL
#define SCB_AIRCR      (*(volatile uint32_t *)0xE000ED0CUL)

#define RCC_AHB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR   (*(volatile uint32_t *)(RCC_BASE + 0x44))

#define GPIO_MODER(base)   (*(volatile uint32_t *)((base) + 0x00))

#define EXTI_IMR       (*(volatile uint32_t *)(EXTI_BASE + 0x00))
#define EXTI_RTSR      (*(volatile uint32_t *)(EXTI_BASE + 0x08))
#define EXTI_PR        (*(volatile uint32_t *)(EXTI_BASE + 0x14))
#define SYSCFG_EXTICR4 (*(volatile uint32_t *)(SYSCFG_BASE + 0x14))

#define NVIC_ISER1      (*(volatile uint32_t *)0xE000E104UL) /* IRQ 32-63 */
#define EXTI15_10_IRQn  40

void EXTI_Init(void)
{
    RCC_AHB1ENR |= (1UL << 2);   /* GPIOC */
    RCC_APB2ENR |= (1UL << 14);  /* SYSCFG */

    /* PC13: input (ไม่ต้องตั้ง pull เพราะบอร์ดมี pull-down ภายนอกอยู่แล้ว) */
    GPIO_MODER(GPIOC_BASE) &= ~(3UL << (13 * 2));

    /* ผูกกับ EXTI13, rising edge (บอร์ดมี pull-down ภายนอก กดแล้วขึ้น 1) */
    SYSCFG_EXTICR4 &= ~(0xFUL << 4);
    SYSCFG_EXTICR4 |=  (2UL   << 4);   /* เลือกพอร์ต C สำหรับ EXTI line 13 */
    EXTI_RTSR |= (1UL << 13);
    EXTI_IMR  |= (1UL << 13);
    NVIC_ISER1 = (1UL << (EXTI15_10_IRQn - 32)) | NVIC_ISER1;
}

/* ชื่อฟังก์ชันต้องตรงกับ vector table ใน startup_stm32f411retx.s */
void EXTI15_10_IRQHandler(void)
{
    if (EXTI_PR & (1UL << 13)) {
        EXTI_PR = (1UL << 13);   /* เคลียร์ pending flag (เขียน 1 เพื่อล้าง) */
        SCB_AIRCR = (0x5FAUL << 16) | (1UL << 2);  /* VECTKEY + SYSRESETREQ */
        for (;;) { }  /* รอรีเซ็ตจริง (ไม่ถึงตรงนี้ในทางปฏิบัติ) */
    }
}
