#include "uart.h"
#include <stdint.h>

/* =====================================================================
 * UART driver สำหรับ STM32F411RE
 * Pin mapping: USART2 TX/RX = PA2 / PA3, AF7 (ขาเดียวกับ ST-Link Virtual COM)
 * ความถี่ระบบสมมติ HSI default 16 MHz — ถ้าตั้ง PLL ไว้ต่างจากนี้ ต้องแก้ SYSTEM_CLOCK_HZ
 * ===================================================================== */

#define SYSTEM_CLOCK_HZ   16000000UL
#define UART_BAUD_RATE    115200UL

#define RCC_BASE          0x40023800UL
#define GPIOA_BASE        0x40020000UL
#define USART2_BASE       0x40004400UL

#define RCC_AHB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x40))

#define GPIO_MODER(base)   (*(volatile uint32_t *)((base) + 0x00))
#define GPIO_AFRL(base)    (*(volatile uint32_t *)((base) + 0x20))

#define USART2_SR    (*(volatile uint32_t *)(USART2_BASE + 0x00))
#define USART2_DR    (*(volatile uint32_t *)(USART2_BASE + 0x04))
#define USART2_BRR   (*(volatile uint32_t *)(USART2_BASE + 0x08))
#define USART2_CR1   (*(volatile uint32_t *)(USART2_BASE + 0x0C))

#define USART_SR_TXE    (1UL << 7)
#define USART_SR_TC     (1UL << 6)
#define USART_CR1_UE    (1UL << 13)
#define USART_CR1_TE    (1UL << 3)
#define USART_CR1_TXEIE (1UL << 7)
#define USART_CR1_TCIE  (1UL << 6)

#define NVIC_ISER1     (*(volatile uint32_t *)0xE000E104UL)  /* IRQ 32-63 */
#define USART2_IRQn    38

static char    uart_buf[100];
static volatile uint8_t uart_len  = 0;
static volatile uint8_t uart_idx  = 0;
static volatile uint8_t uart_busy = 0;

void UART_Init(void)
{
    RCC_AHB1ENR |= (1UL << 0);   /* GPIOA */
    RCC_APB1ENR |= (1UL << 17);  /* USART2 */

    /* PA2(TX)=AF7, PA3(RX)=AF7 */
    GPIO_MODER(GPIOA_BASE) &= ~((3UL << (2 * 2)) | (3UL << (3 * 2)));
    GPIO_MODER(GPIOA_BASE) |=  ((2UL << (2 * 2)) | (2UL << (3 * 2))); /* 10 = alternate function */
    GPIO_AFRL(GPIOA_BASE)  &= ~((0xFUL << (2 * 4)) | (0xFUL << (3 * 4)));
    GPIO_AFRL(GPIOA_BASE)  |=  ((7UL   << (2 * 4)) | (7UL   << (3 * 4))); /* AF7 = USART2 */

    USART2_BRR = (SYSTEM_CLOCK_HZ + (UART_BAUD_RATE / 2)) / UART_BAUD_RATE;
    USART2_CR1 = USART_CR1_UE | USART_CR1_TE;
    NVIC_ISER1 = (1UL << (USART2_IRQn - 32));
}

/* ชื่อฟังก์ชันต้องตรงกับ vector table ใน startup_stm32f411retx.s */
void USART2_IRQHandler(void)
{
    if (USART2_SR & USART_SR_TXE) {
        if (uart_idx < uart_len) {
            USART2_DR = (uint32_t)uart_buf[uart_idx++];
        } else {
            USART2_CR1 &= ~USART_CR1_TXEIE;
            USART2_CR1 |=  USART_CR1_TCIE;
        }
    }
    if (USART2_SR & USART_SR_TC) {
        USART2_CR1 &= ~USART_CR1_TCIE;
        USART2_SR  &= ~USART_SR_TC;
        uart_busy = 0;
    }
}

void BSP_UART_Print(const char *text)
{
    while (uart_busy) {
        /* รอการส่งครั้งก่อนเสร็จ (flag ถูกเคลียร์จาก ISR ไม่ใช่การ polling ฮาร์ดแวร์) */
    }

    uint8_t len = 0;
    while (text[len] != '\0' && len < (uint8_t)(sizeof(uart_buf) - 2)) {
        uart_buf[len] = text[len];
        len++;
    }
    uart_buf[len]     = '\r';
    uart_buf[len + 1] = '\n';

    uart_len  = (uint8_t)(len + 2);
    uart_idx  = 0;
    uart_busy = 1;

    USART2_CR1 |= USART_CR1_TXEIE;
}
