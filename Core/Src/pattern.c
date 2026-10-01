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
    /* ต้องตรงกับสีไฟจริงบนชิลด์ STEO ตามลำดับขาใน led.c:
     *   ดวงที่ 1 = PA5 = D13 = ฟ้า
     *   ดวงที่ 2 = PA6 = D12 = แดง
     *   ดวงที่ 3 = PA7 = D11 = เหลือง
     *   ดวงที่ 4 = PB6 = D10 = เขียว
     * ของเดิมใส่ชื่อสลับ (1=RED) ทำให้ข้อความ "Don't press" ระบุสีผิดไปคนละดวง */
    switch (color) {
        case 1:  return "BLUE";
        case 2:  return "RED";
        case 3:  return "YELLOW";
        case 4:  return "GREEN";
        default: return "NONE";
    }
}
