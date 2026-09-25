#include "challenge.h"
#include "pattern.h"
#include "random.h"
#include "buzzer.h"
#include "uart.h"
#include "timer.h"
#include <stdio.h>

#define BUZZ_DURATION_MS   500

uint8_t Challenge_RollNoPress(uint8_t *out_forbidden_color)
{
    if (BSP_Random_Range(0, 99) < NO_PRESS_PROBABILITY_PCT) {
        *out_forbidden_color = BSP_Random_Range(1, 4); /* สุ่มสีต้องห้าม 1 ใน 4 สี */
        return 1;
    }
    *out_forbidden_color = 0;
    return 0;
}

uint8_t Challenge_RollBuzz(void)
{
    if (BSP_Random_Range(0, 99) < BUZZ_PROBABILITY_PCT) {
        BSP_Buzzer_Beep(BUZZ_DURATION_MS);  /* ส่งเสียงเตือนทันทีที่เข้าเงื่อนไข */
        return 1;
    }
    return 0;
}

void Challenge_AnnounceRoundInfo(uint8_t forbidden_color, uint8_t is_reverse)
{
    char line[32];

    if (forbidden_color != 0) {
        snprintf(line, sizeof(line), "Don't press: %s", Pattern_ColorName(forbidden_color));
    } else {
        snprintf(line, sizeof(line), "Don't press: NONE");
    }
    BSP_UART_Print(line);
    BSP_UART_Print(is_reverse ? "Reverse: YES" : "Reverse: NO");
}

void Challenge_DisplayCountdown(uint8_t seconds)
{
    char line[16];
    for (uint8_t s = seconds; s > 0; s--) {
        snprintf(line, sizeof(line), "%u", s);
        BSP_UART_Print(line);
        BSP_Delay_ms(1000);
    }
}

void Challenge_BuildExpectedAnswer(const Pattern_t *question, uint8_t reverse,
                                    uint8_t forbidden_color, Pattern_t *expected_out)
{
    Pattern_Clear(expected_out);

    for (uint8_t i = 0; i < question->length; i++) {
        uint8_t idx = reverse ? (uint8_t)(question->length - 1 - i) : i;
        uint8_t color = question->data[idx];

        if (color == forbidden_color) {
            continue; /* ตัดตำแหน่งที่เป็นสีต้องห้ามออก ไม่ต้องกดตำแหน่งนี้ */
        }

        expected_out->data[expected_out->length] = color;
        expected_out->length++;
    }
}
