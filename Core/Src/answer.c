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

    while (answer->length < expected->length) {
        uint8_t btn = BSP_Button_WaitPress(); /* บล็อกรอจนกว่าจะมีการกดปุ่มใหม่ */

        if (forbidden_color != 0 && btn == forbidden_color) {
            wrong_count++;
            continue; /* กดสีต้องห้าม: ผิด 1 ครั้ง แต่ไม่กินตำแหน่งคำตอบ รอปุ่มถัดไป */
        }

        if (btn < 1 || btn > 4) {
            continue; /* ค่าที่อ่านไม่ได้ ไม่ควรเกิดขึ้นจริง ข้ามไปเฉยๆ */
        }

        if (btn != expected->data[answer->length]) {
            wrong_count++; /* กดสีอื่นผิดตำแหน่ง: ผิด 1 ครั้ง แต่ยังคงขยับตำแหน่งต่อ */
        }

        answer->data[answer->length] = btn;
        answer->length++;
    }

    return wrong_count;
}
