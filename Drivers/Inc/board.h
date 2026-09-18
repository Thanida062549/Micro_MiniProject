#ifndef BOARD_H
#define BOARD_H

/* เรียกครั้งเดียวตอนต้น main() ก่อน Game_Init() เสมอ
 * ทำหน้าที่เปิด driver ของทุกอุปกรณ์ (timer, led, button, buzzer, uart, adc, exti)
 * ตามลำดับที่ถูกต้อง แล้ว seed ตัวสุ่มเลขด้วยค่าจาก ADC + เวลา ณ ตอนบูต */
void BSP_Init(void);

#endif /* BOARD_H */
