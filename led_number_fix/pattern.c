#include "pattern.h"
#include "led.h"
#include "timer.h"
#include "random.h"

#define LED_ON_MS   400   /* ระยะเวลาไฟติดต่อ 1 ดวงในแพทเทิร์น */
#define LED_GAP_MS  200   /* ช่วงพักไฟดับระหว่างดวง ให้แยกจังหวะออกจากกันชัดเจน */

void Pattern_Clear(Pattern_t *p)
{
    for (uint8_t i = 0; i < PATTERN_MAX_LEN; i++) {
        p->data[i] = 0;
    }
    p->length = 0;
}

uint8_t Pattern_LengthForDifficulty(Difficulty_t difficulty)
{
    switch (difficulty) {
        case DIFFICULTY_EASY: return 4;
        case DIFFICULTY_HARD: return 8;
        case DIFFICULTY_MEDIUM:
        default:               return 6;
    }
}

void Pattern_Generate(Pattern_t *p, uint8_t length)
{
    Pattern_Clear(p);
    p->length = length;
    for (uint8_t i = 0; i < length; i++) {
        p->data[i] = BSP_Random_Range(1, 4); /* สุ่มสี/LED ดวงที่ 1-4 */
    }
}

void Pattern_Show(const Pattern_t *p)
{
    for (uint8_t i = 0; i < p->length; i++) {
        BSP_LED_On(p->data[i]);
        BSP_Delay_ms(LED_ON_MS);
        BSP_LED_Off(p->data[i]);
        BSP_Delay_ms(LED_GAP_MS);
    }
}

const char *Pattern_ColorName(uint8_t color)
{
    switch (color) {
        case 1:  return "RED";
        case 2:  return "YELLOW";
        case 3:  return "GREEN";
        case 4:  return "BLUE";
        default: return "NONE";
    }
}
