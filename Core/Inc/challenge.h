#ifndef CHALLENGE_H
#define CHALLENGE_H

#include "game_types.h"

/* สุ่มว่าด่านนี้มีสีต้องห้ามหรือไม่ (สุ่มใหม่ทุกด่าน ไม่สะสมจากด่านก่อน)
 * คืนค่า 1 พร้อมเขียนสีต้องห้าม (1-4) ลง *out_forbidden_color ถ้าเข้าเงื่อนไข
 * คืนค่า 0 พร้อม *out_forbidden_color = 0 ถ้าไม่เข้าเงื่อนไข */
uint8_t Challenge_RollNoPress(uint8_t *out_forbidden_color);

/* สุ่มว่าด่านนี้ buzzer ดังหรือไม่ (สุ่มใหม่ทุกด่าน) ถ้าดัง (1) จะสั่งเสียง buzzer ทันที
 * และต้องตอบย้อนลำดับ */
uint8_t Challenge_RollBuzz(void);

/* ประกาศข้อมูล challenge ของด่านนี้ไปที่ PC ผ่าน UART เสมอ (ไม่ว่าจะมี challenge หรือไม่)
 * เรียกหลังนับถอยหลังจบ ก่อนไฟ LED เริ่มเล่นแพทเทิร์น */
void Challenge_AnnounceRoundInfo(uint8_t forbidden_color, uint8_t is_reverse);

/* แสดง countdown นับถอยหลังไปที่ PC ผ่าน UART เป็นจำนวนวินาทีที่กำหนด */
void Challenge_DisplayCountdown(uint8_t seconds);

/* สร้างลำดับคำตอบที่ถูกต้อง โดย:
 *  - ย้อนลำดับก่อนถ้า reverse = 1
 *  - ตัดตำแหน่งที่เป็นสีต้องห้าม (forbidden_color) ออก ถ้ามี (forbidden_color != 0)
 * ผลลัพธ์เก็บไว้ใน *expected_out */
void Challenge_BuildExpectedAnswer(const Pattern_t *question, uint8_t reverse,
                                    uint8_t forbidden_color, Pattern_t *expected_out);

#endif /* CHALLENGE_H */
