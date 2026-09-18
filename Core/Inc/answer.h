#ifndef ANSWER_H
#define ANSWER_H

#include "game_types.h"

/* เคลียร์ answer pattern ให้ว่างเปล่า */
void Answer_Clear(Pattern_t *answer);

/* รับปุ่มที่ผู้เล่นกด จนกว่าจะตอบครบทุกตำแหน่งใน *expected (ไม่มีปุ่ม Submit)
 * กติกา:
 *  - กดสีต้องห้าม (forbidden_color, 0 = ไม่มีข้อจำกัด) -> นับผิด 1 ครั้ง แต่ไม่กินตำแหน่งคำตอบ เล่นต่อ
 *  - กดสีอื่นผิดตำแหน่ง (ไม่ตรงกับ expected ตำแหน่งปัจจุบัน) -> นับผิด 1 ครั้ง และขยับไปตำแหน่งถัดไป
 *  - กดถูกตำแหน่ง -> ขยับไปตำแหน่งถัดไปตามปกติ ไม่ตัดจบด่านไม่ว่ากรณีใด
 * คืนค่าจำนวนครั้งทั้งหมดที่กดผิดในด่านนี้ (0 = ตอบถูกทั้งหมด) */
uint8_t Answer_PlayRound(Pattern_t *answer, const Pattern_t *expected, uint8_t forbidden_color);

#endif /* ANSWER_H */
