#ifndef ADC_H
#define ADC_H

#include <stdint.h>

/* เปิด clock + ตั้งค่า GPIO/ADC1 แล้วสั่งให้แปลงต่อเนื่องอยู่เบื้องหลัง
 * (แจ้งผลด้วย interrupt ทุกครั้งที่แปลงเสร็จ) — เรียกครั้งเดียวจาก board.c */
void ADC_Init(void);

/* คืนค่า ADC ล่าสุด (0-4095) ทันที ไม่มีการสั่งแปลงและไม่มีการรอ
 * ใช้เป็น entropy ให้ random.c ได้ด้วย */
uint16_t ADC_ReadRaw(void);

/* อ่านค่า potentiometer แล้วแบ่งเป็น 3 โซน คืนค่า 0=EASY, 1=MEDIUM, 2=HARD
 * (ตรงกับ Difficulty_t ใน game_types.h) เรียกใหม่ทุกครั้งที่เริ่มเกม */
uint8_t BSP_ReadDifficulty(void);

#endif /* ADC_H */
