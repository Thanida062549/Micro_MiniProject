#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

/* เปิด clock + ตั้งค่า GPIO ของ buzzer เป็น output — เรียกครั้งเดียวจาก board.c */
void Buzzer_Init(void);

/* สั่งให้ buzzer ดังเป็นเวลา duration_ms แล้วดับ (on/off ธรรมดา ไม่มีโทนเสียง) */
void BSP_Buzzer_Beep(uint16_t duration_ms);

#endif /* BUZZER_H */
