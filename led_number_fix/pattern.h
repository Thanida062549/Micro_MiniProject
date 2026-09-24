#ifndef PATTERN_H
#define PATTERN_H

#include "game_types.h"

/* เคลียร์ pattern ให้ว่างเปล่า (ใช้ทั้งกับ question และ answer) */
void Pattern_Clear(Pattern_t *p);

/* ความยาว pattern คงที่ตามระดับความยาก (เท่ากันทุกด่าน): EASY=4, MEDIUM=6, HARD=8 */
uint8_t Pattern_LengthForDifficulty(Difficulty_t difficulty);

/* สุ่ม pattern ใหม่ความยาวตามที่กำหนด (ใช้สร้าง Store_Question_Pattern) */
void Pattern_Generate(Pattern_t *p, uint8_t length);

/* แสดง pattern จริงบน LED ทีละดวงตามลำดับ (ใช้ตอน state: show_pattern) */
void Pattern_Show(const Pattern_t *p);

/* คืนชื่อสีเป็นข้อความ (1=RED 2=YELLOW 3=GREEN 4=BLUE) สำหรับส่งไป UART */
const char *Pattern_ColorName(uint8_t color);

#endif /* PATTERN_H */
