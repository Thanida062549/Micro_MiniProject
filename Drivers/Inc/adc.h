#ifndef ADC_H
#define ADC_H

#include <stdint.h>

/* เปิด clock + ตั้งค่า GPIO/ADC1 (channel, interrupt) — เรียกครั้งเดียวจาก board.c */
void ADC_Init(void);

/* อ่านค่า ADC ดิบ 1 ครั้งแบบ interrupt-driven (0-4095) ใช้เป็น entropy ให้ random.c ได้ด้วย */
uint16_t ADC_ReadRaw(void);

/* อ่านค่า potentiometer แล้วแบ่งเป็น 3 โซน คืนค่า 0=EASY, 1=MEDIUM, 2=HARD
 * (ตรงกับ Difficulty_t ใน game_types.h) เรียกใหม่ทุกครั้งที่เริ่มเกม */
uint8_t BSP_ReadDifficulty(void);

#endif /* ADC_H */
