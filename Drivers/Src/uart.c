#include "uart.h"
#include <stdint.h>

/* =====================================================================
 * UART driver สำหรับ STM32F411RE
 * Pin mapping: USART2 TX/RX = PA2 / PA3, AF7 (ขาเดียวกับ ST-Link Virtual COM)
 * ความถี่ระบบสมมติ HSI default 16 MHz — ถ้าตั้ง PLL ไว้ต่างจากนี้ ต้องแก้ SYSTEM_CLOCK_HZ
 *
 * การส่งเป็น interrupt แบบไม่มีการรอเลย:
 *   BSP_UART_Print() แค่ใส่ข้อความลงคิว (ring buffer) แล้วกลับทันที
 *   ส่วน USART2_IRQHandler() ทยอยดึงจากคิวไปส่งทีละไบต์ เมื่อ USART แจ้งว่า
 *   พร้อมรับไบต์ถัดไป (TXE) ไม่มีลูปที่วนรอ flag ทั้งในโปรแกรมหลักและใน ISR
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
#define USART_CR1_UE    (1UL << 13)
#define USART_CR1_TE    (1UL << 3)
#define USART_CR1_TXEIE (1UL << 7)

#define NVIC_ISER1     (*(volatile uint32_t *)0xE000E104UL)  /* IRQ 32-63 */
#define USART2_IRQn    38

/* ---------------------------------------------------------------------
 * คิวส่งข้อมูล (ring buffer)
 *
 * ขนาดต้องเป็นกำลังของ 2 และหาร 65536 ลงตัว เพื่อให้ตัวชี้แบบ uint16_t
 * ที่นับต่อเนื่องแล้ววนกลับเป็น 0 เอง ยังคำนวณจำนวนไบต์ในคิวได้ถูกต้อง
 *
 * ผู้เขียนมีคนเดียวคือโปรแกรมหลัก (แก้เฉพาะ tx_head)
 * ผู้อ่านมีคนเดียวคือ ISR              (แก้เฉพาะ tx_tail)
 * ต่างคนต่างแก้ตัวชี้คนละตัว จึงไม่ต้องปิด interrupt ระหว่างเข้าถึงคิว
 * --------------------------------------------------------------------- */
#define UART_TX_BUF_SIZE   512u
#define UART_TX_BUF_MASK   (UART_TX_BUF_SIZE - 1u)

static volatile uint8_t  tx_buf[UART_TX_BUF_SIZE];
static volatile uint16_t tx_head    = 0;   /* ตำแหน่งที่จะเขียนไบต์ถัดไป (แก้โดยโปรแกรมหลัก) */
static volatile uint16_t tx_tail    = 0;   /* ตำแหน่งที่จะอ่านไบต์ถัดไป   (แก้โดย ISR)        */
static volatile uint16_t tx_dropped = 0;   /* จำนวนข้อความที่ถูกทิ้งเพราะคิวเต็ม (ไว้ตรวจสอบ) */

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
    USART2_CR1 = USART_CR1_UE | USART_CR1_TE;   /* TXEIE ยังปิดอยู่ จะเปิดเมื่อมีข้อความในคิว */
    NVIC_ISER1 = (1UL << (USART2_IRQn - 32));
}

/* ชื่อฟังก์ชันต้องตรงกับ vector table ใน startup_stm32f411retx.s
 *
 * ทำงานเมื่อ USART แจ้งว่า "พร้อมรับไบต์ถัดไป" (TXE) เท่านั้น
 *   - มีข้อมูลในคิว  -> ส่ง 1 ไบต์
 *   - คิวว่าง        -> ปิด TXEIE เอง จะได้ไม่ถูกเรียกซ้ำโดยไม่มีอะไรให้ส่ง
 * ต้องเช็คบิต TXEIE คู่กับ flag เสมอ เพราะ TXE เป็น 1 อยู่แล้วตั้งแต่หลังรีเซ็ต */
void USART2_IRQHandler(void)
{
    if ((USART2_CR1 & USART_CR1_TXEIE) && (USART2_SR & USART_SR_TXE)) {
        uint16_t tail = tx_tail;

        if (tail != tx_head) {
            USART2_DR = (uint32_t)tx_buf[tail & UART_TX_BUF_MASK];  /* เขียน DR จะล้าง TXE เอง */
            tx_tail = (uint16_t)(tail + 1u);
        } else {
            USART2_CR1 &= ~USART_CR1_TXEIE;
        }
    }
}

/* ใส่ข้อความ 1 บรรทัด (ต่อท้ายด้วย \r\n) ลงคิวส่ง แล้วกลับทันที ไม่รอให้ส่งเสร็จ
 *
 * ถ้าคิวเหลือที่ไม่พอสำหรับทั้งข้อความ จะทิ้งข้อความนั้นทั้งบรรทัดแล้วนับไว้ใน
 * tx_dropped (ไม่ส่งครึ่งๆ กลางๆ และไม่รอ) กรณีนี้ไม่ควรเกิดในเกมจริง เพราะ
 * ข้อความห่างกันด้วย delay อยู่แล้ว และคิวจุได้ 512 ไบต์ */
void BSP_UART_Print(const char *text)
{
    uint16_t len = 0;
    while (text[len] != '\0') {
        len++;
    }
    uint16_t need = (uint16_t)(len + 2u);          /* + \r\n */

    uint16_t head  = tx_head;
    uint16_t used  = (uint16_t)(head - tx_tail);   /* tx_tail ที่อ่านได้อาจเก่ากว่าจริง = ปลอดภัย
                                                      เพราะ tail มีแต่เดินหน้า พื้นที่ว่างจริง
                                                      จึงมากกว่าหรือเท่ากับที่คำนวณได้เสมอ */
    uint16_t space = (uint16_t)(UART_TX_BUF_SIZE - used);

    if (need > space) {
        tx_dropped++;
        return;
    }

    for (uint16_t i = 0; i < len; i++) {
        tx_buf[head & UART_TX_BUF_MASK] = (uint8_t)text[i];
        head++;
    }
    tx_buf[head & UART_TX_BUF_MASK] = (uint8_t)'\r';
    head++;
    tx_buf[head & UART_TX_BUF_MASK] = (uint8_t)'\n';
    head++;

    tx_head = head;                 /* ประกาศให้ ISR เห็นข้อมูลใหม่ "หลัง" เขียนเสร็จทั้งหมด */
    USART2_CR1 |= USART_CR1_TXEIE;  /* เปิดให้ ISR เริ่ม/ส่งต่อ */
}
