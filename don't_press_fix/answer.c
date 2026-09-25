#include "answer.h"
#include "button.h"

void Answer_Clear(Pattern_t *answer)
{
    for (uint8_t i = 0; i < PATTERN_MAX_LEN; i++) {
        answer->data[i] = 0;
    }
    answer->length = 0;
}

uint8_t Answer_PlayRound(Pattern_t *answer, const Pattern_t *expected, uint8_t forbidden_color)
{
    Answer_Clear(answer);
    uint8_t wrong_count = 0;

    /* forbidden_color ไม่ต้องเช็คแยกอีกแล้ว:
     * expected ถูกตัดตำแหน่งของสีต้องห้ามออกไปตั้งแต่ Challenge_BuildExpectedAnswer
     * การกดสีต้องห้ามจึงไม่มีทางตรงกับ expected อยู่แล้ว -> ถูกนับเป็นผิดโดยอัตโนมัติ
     *
     * ของเดิมใช้ continue ทำให้การกดสีต้องห้าม "ไม่กินตำแหน่ง" ผู้เล่นจึงต้องกดเพิ่ม
     * ไปเรื่อยๆ ไม่จบ และ wrong_count พุ่งเกินจำนวนช่องได้ (เช่น 10 wrong ในด่าน 6 ดวง) */
    (void)forbidden_color;

    while (answer->length < expected->length) {
        uint8_t btn = BSP_Button_WaitPress(); /* บล็อกรอจนกว่าจะมีการกดปุ่มใหม่ */

        if (btn < 1 || btn > 4) {
            continue; /* ค่าที่อ่านไม่ได้ ไม่ควรเกิดขึ้นจริง ข้ามไปเฉยๆ */
        }

        if (btn != expected->data[answer->length]) {
            wrong_count++; /* กดไม่ตรงกับที่ควรกด (รวมถึงกดสีต้องห้าม) = ผิด 1 ครั้ง */
        }

        /* ทุกการกดที่อ่านค่าได้ กินตำแหน่ง 1 ช่องเสมอ */
        answer->data[answer->length] = btn;
        answer->length++;
    }

    return wrong_count;
}
