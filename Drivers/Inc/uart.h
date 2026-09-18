#ifndef UART_H
#define UART_H

/* เปิด clock + ตั้งค่า GPIO/USART2 (baud rate, interrupt) — เรียกครั้งเดียวจาก board.c */
void UART_Init(void);

/* ส่งข้อความ 1 บรรทัดออกไปที่ PC ผ่าน USART2 แบบ interrupt (ห้าม polling) */
void BSP_UART_Print(const char *text);

#endif /* UART_H */
