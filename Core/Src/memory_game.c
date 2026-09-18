#include "memory_game.h"
#include "pattern.h"
#include "challenge.h"
#include "answer.h"
#include "hp.h"
#include "uart.h"
#include "button.h"
#include "adc.h"
#include "timer.h"
#include <stdio.h>

void MemoryGame_ResetForNewGame(GameContext_t *ctx)
{
    /* ตั้งค่าเริ่มต้นทั้งเกม */
    ctx->round = 0;
    HP_Init(ctx);
    ctx->accuracy_count = 0;
    Pattern_Clear(&ctx->question);
    Pattern_Clear(&ctx->answer);

    /* --- หน้าเลือกความยาก: หมุน potentiometer แล้วกดปุ่ม 1 (PA10) เพื่อยืนยัน ---
     * กดปุ่มอื่นระหว่างนี้ไม่มีผลอะไร วนรอปุ่ม 1 เท่านั้น */
    BSP_UART_Print("Turn the dial to set difficulty, then press button 1 to confirm.");
    while (BSP_Button_WaitPress() != 1) {
        /* รอปุ่ม 1 เท่านั้น */
    }
    ctx->difficulty = (Difficulty_t)BSP_ReadDifficulty();

    {
        const char *name = "MEDIUM";
        if (ctx->difficulty == DIFFICULTY_EASY) { name = "EASY"; }
        if (ctx->difficulty == DIFFICULTY_HARD) { name = "HARD"; }
        char line[24];
        snprintf(line, sizeof(line), "Difficulty: %s", name);
        BSP_UART_Print(line);
    }

    /* --- เริ่มด่านที่ 1: Round 1 -> นับถอยหลัง 3 2 1 (รวม ~4 วิ) --- */
    BSP_UART_Print("Round 1");
    BSP_Delay_ms(1000);
    Challenge_DisplayCountdown(3);
}

RoundOutcome_t MemoryGame_PlayRound(GameContext_t *ctx)
{
    Pattern_t expected;
    char line[24];

    ctx->round++;
    Pattern_Clear(&ctx->question);
    Pattern_Clear(&ctx->answer);

    /* ---- สุ่ม challenge ของด่านนี้ใหม่ทุกครั้ง ไม่สะสมจากด่านก่อน ---- */
    Challenge_RollNoPress(&ctx->no_press_color);
    ctx->is_reverse_round = Challenge_RollBuzz();

    /* ---- ประกาศ challenge ก่อนไฟ LED เริ่มเล่น (หลังนับถอยหลังจบพอดี) ---- */
    Challenge_AnnounceRoundInfo(ctx->no_press_color, ctx->is_reverse_round);

    /* ---- แสดงแพทเทิร์น: ความยาวคงที่ตามความยาก ไม่ไต่ระดับต่อด่านแล้ว ---- */
    uint8_t len = Pattern_LengthForDifficulty(ctx->difficulty);
    Pattern_Generate(&ctx->question, len);
    Pattern_Show(&ctx->question);

    /* ---- เตรียมคำตอบที่ถูกต้อง (ย้อนลำดับ + ตัดสีต้องห้าม ถ้ามี) ---- */
    Challenge_BuildExpectedAnswer(&ctx->question, ctx->is_reverse_round,
                                  ctx->no_press_color, &expected);

    /* ---- รับคำตอบ: เล่นต่อจนครบเสมอ นับจำนวนครั้งที่กดผิด ไม่ตัดจบกลางคัน ---- */
    uint8_t wrong_count = Answer_PlayRound(&ctx->answer, &expected, ctx->no_press_color);

    snprintf(line, sizeof(line), "You got %u wrong.", wrong_count);
    BSP_UART_Print(line);

    if (wrong_count == 0) {
        ctx->accuracy_count++;
    } else {
        HP_Decrement(ctx); /* ด่านนี้มีกดผิดอย่างน้อย 1 ครั้ง -> hp -1 (ไม่สนจำนวนครั้ง) */
    }

    if (HP_IsGameOver(ctx)) {
        return ROUND_GAME_OVER;
    }
    if (ctx->round >= TOTAL_ROUNDS) {
        return ROUND_ACCURACY_DONE; /* จบด่าน 5 แล้ว ไม่มี "Round ถัดไป" อีก ไปหน้าสรุปผลเลย */
    }

    /* ---- มีด่านถัดไป: พิมพ์เลขด่าน + นับถอยหลัง ก่อนจบฟังก์ชันนี้ ---- */
    snprintf(line, sizeof(line), "Round %u", ctx->round + 1);
    BSP_UART_Print(line);
    BSP_Delay_ms(1000);
    Challenge_DisplayCountdown(3);

    return ROUND_CONTINUE;
}
