#include "exti.h"
#include <stdint.h>

/* =========================
 * Base Addresses
 * ========================= */

#define RCC_BASE        0x40023800UL
#define GPIOC_BASE      0x40020800UL
#define EXTI_BASE       0x40013C00UL
#define SYSCFG_BASE     0x40013800UL

/* =========================
 * RCC Registers
 * ========================= */

#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x44))

/* =========================
 * GPIOC Registers
 * ========================= */

#define GPIOC_MODER     (*(volatile uint32_t *)(GPIOC_BASE + 0x00))
#define GPIOC_PUPDR     (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))

/* =========================
 * SYSCFG
 * ========================= */

#define SYSCFG_EXTICR4  (*(volatile uint32_t *)(SYSCFG_BASE + 0x14))

/* =========================
 * EXTI Registers
 * ========================= */

#define EXTI_IMR        (*(volatile uint32_t *)(EXTI_BASE + 0x00))
#define EXTI_RTSR       (*(volatile uint32_t *)(EXTI_BASE + 0x08))
#define EXTI_FTSR       (*(volatile uint32_t *)(EXTI_BASE + 0x0C))
#define EXTI_PR         (*(volatile uint32_t *)(EXTI_BASE + 0x14))

/* =========================
 * NVIC
 * ========================= */

#define NVIC_ISER1      (*(volatile uint32_t *)0xE000E104UL)

/* =========================
 * System Control Block
 * ========================= */

#define SCB_AIRCR       (*(volatile uint32_t *)0xE000ED0CUL)

/* =========================
 * PC12 / EXTI12
 * ========================= */

#define PC12_BIT        (1UL << 12)

/*
 * EXTI12 belongs to EXTI15_10_IRQn
 *
 * IRQ number = 40
 * NVIC ISER1 bit = 40 - 32 = 8
 */
#define EXTI15_10_IRQ_BIT   (1UL << 8)


/* =========================================================
 * EXTI Initialization
 * ========================================================= */

void EXTI_Init(void)
{
    /* Enable GPIOC clock */
    RCC_AHB1ENR |= (1UL << 2);

    /* Enable SYSCFG clock */
    RCC_APB2ENR |= (1UL << 14);


    /* -----------------------------------------------------
     * PC12 = Input
     *
     * MODER12 = 00
     * ----------------------------------------------------- */

    GPIOC_MODER &= ~(3UL << (12 * 2));


    /* -----------------------------------------------------
     * PC12 = Pull-up
     *
     * 01 = Pull-up
     * ----------------------------------------------------- */

    GPIOC_PUPDR &= ~(3UL << (12 * 2));
    GPIOC_PUPDR |=  (1UL << (12 * 2));


    /* -----------------------------------------------------
     * EXTI12 -> GPIOC
     *
     * EXTI12 is in EXTICR4
     * bits [3:0]
     *
     * 0010 = GPIOC
     * ----------------------------------------------------- */

    SYSCFG_EXTICR4 &= ~(0xFUL << 0);
    SYSCFG_EXTICR4 |=  (0x2UL << 0);


    /* -----------------------------------------------------
     * Falling edge only
     *
     * HIGH -> LOW = interrupt
     * ----------------------------------------------------- */

    EXTI_RTSR &= ~PC12_BIT;
    EXTI_FTSR |=  PC12_BIT;


    /* -----------------------------------------------------
     * Clear old pending flag
     * ----------------------------------------------------- */

    EXTI_PR = PC12_BIT;


    /* -----------------------------------------------------
     * Enable EXTI12 interrupt
     * ----------------------------------------------------- */

    EXTI_IMR |= PC12_BIT;


    /* -----------------------------------------------------
     * Enable EXTI15_10 in NVIC
     * ----------------------------------------------------- */

    NVIC_ISER1 |= EXTI15_10_IRQ_BIT;
}


/* =========================================================
 * EXTI15_10 Interrupt Handler
 * ========================================================= */

void EXTI15_10_IRQHandler(void)
{
    /*
     * Check whether EXTI12 caused the interrupt
     */
    if (EXTI_PR & PC12_BIT)
    {
        /*
         * Clear pending flag
         */
        EXTI_PR = PC12_BIT;


        /*
         * System Reset
         *
         * Key = 0x5FA
         * SYSRESETREQ = bit 2
         */
        SCB_AIRCR = (0x5FAUL << 16) | (1UL << 2);


        /*
         * Wait until MCU resets
         */
        while (1)
        {
        }
    }
}
