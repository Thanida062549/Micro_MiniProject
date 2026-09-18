#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

/* เปิด clock + ตั้งค่า GPIO ของปุ่มตอบ 4 ปุ่มเป็น input พร้อม pull-up — เรียกครั้งเดียวจาก board.c */
void Button_Init(void);

/* คืนค่า 0 = ไม่มีปุ่มไหนถูกกด, 1-4 = ปุ่มตอบ (ยังไม่ debounce) */
uint8_t BSP_Button_Read(void);

/* บล็อกรอจนกว่าจะมีการกดปุ่มใหม่ (debounce แล้ว) แล้วคืนค่ารหัสปุ่มนั้น */
uint8_t BSP_Button_WaitPress(void);

#endif /* BUTTON_H */
