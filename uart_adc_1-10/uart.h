#ifndef UART_H
#define UART_H

/* เปิด clock + ตั้งค่า GPIO/USART2 (baud rate, interrupt) — เรียกครั้งเดียวจาก board.c */
void UART_Init(void);

/* ใส่ข้อความ 1 บรรทัดลงคิวส่งของ USART2 แล้วกลับทันที ไม่รอให้ส่งเสร็จ
 * ไบต์จริงถูกส่งออกโดย interrupt ทีละตัว (ไม่มี polling และไม่มีลูปรอ)
 * ถ้าคิวเต็มจะทิ้งข้อความนั้นทั้งบรรทัดแทนที่จะรอ */
void BSP_UART_Print(const char *text);

#endif /* UART_H */
