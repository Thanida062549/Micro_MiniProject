#include "result.h"
#include "uart.h"
#include "timer.h"
#include <stdio.h>

#define SUMMARY_HOLD_MS  5000  /* ค้างผลไว้ 5 วิ ตอนสรุปผล/จบเกม */

void Result_ShowAccuracy(uint8_t accuracy_count, uint8_t total_rounds)
{
    uint8_t wrong_rounds = (uint8_t)(total_rounds - accuracy_count);
    uint16_t pct = (uint16_t)((accuracy_count * 100u) / total_rounds);
    char line[96];

    if (wrong_rounds == 0) {
        snprintf(line, sizeof(line),
                 "You got everything correct in all rounds! Congratulations. Your accuracy is %u%%.",
                 pct);
    } else {
        snprintf(line, sizeof(line),
                 "Out of %u rounds, you got %u wrong. Your accuracy is %u%%.",
                 total_rounds, wrong_rounds, pct);
    }

    BSP_UART_Print(line);
    BSP_Delay_ms(SUMMARY_HOLD_MS);
}

void Result_ShowGameOver(void)
{
    BSP_UART_Print("GAME OVER");
    BSP_Delay_ms(SUMMARY_HOLD_MS);
}
