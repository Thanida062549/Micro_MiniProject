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

#define ROUND_INFO_HOLD_MS    3000  /* ค้าง Don't press / Reverse ให้อ่านทัน ก่อนนับถอยหลัง */
#define ROUND_RESULT_HOLD_MS  5000  /* ค้างผลท้ายด่านให้อ่านทัน ก่อนขึ้นด่านถัดไป */

void MemoryGame_ResetForNewGame(GameContext_t *ctx)
{
    /* ตั้งค่าเริ่มต้นทั้งเกม */
    ctx->round = 0;
    HP_Init(ctx);
    ctx->accuracy_count = 0;
    Pattern_Clear(&ctx->question);
    Pattern_Clear(&ctx->answer);

    /* --- หน้าเริ่มเกม ---
     * วางไว้ที่นี่ ไม่ใช่ main() เพราะ main() รันครั้งเดียวตอนบูต
     * แต่เกมวนเริ่มใหม่ผ่าน Game_Init() ซึ่งไม่กลับไปที่ main() อีก */
    BSP_UART_Print("Welcome to Memory Pattern Master Game!");

    /* --- หน้าเลือกความยาก: หมุน potentiometer แล้วกดปุ่ม 1 (PA10) เพื่อยืนยัน ---
     * กดปุ่มอื่นระหว่างนี้ไม่มีผลอะไร วนรอปุ่ม 1 เท่านั้น */
    BSP_UART_Print("Turn the dial to set difficulty, then press BUTTON 1 (the first button from the left) to confirm.");
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

    /* ไม่พิมพ์ "Round 1" หรือนับถอยหลังตรงนี้
     * ทุกด่าน (รวมด่านแรก) จัดการครบใน MemoryGame_PlayRound แล้ว */
}

RoundOutcome_t MemoryGame_PlayRound(GameContext_t *ctx)
{
    Pattern_t expected;
    char line[32];

    ctx->round++;
    Pattern_Clear(&ctx->question);
    Pattern_Clear(&ctx->answer);

    /* ---- ประกาศเลขด่าน ---- */
    snprintf(line, sizeof(line), "Round %u", ctx->round);
    BSP_UART_Print(line);
    BSP_Delay_ms(1000);

    /* ---- สุ่ม challenge ของด่านนี้ใหม่ทุกครั้ง ไม่สะสมจากด่านก่อน ---- */
    Challenge_RollNoPress(&ctx->no_press_color);
    ctx->is_reverse_round = Challenge_RollBuzz();

    /* ---- สร้างแพทเทิร์นและคำตอบที่ถูกต้องไว้เงียบๆ ก่อน (ยังไม่โชว์ไฟ ยังไม่เฉลย) ----
     * ไม่บอกจำนวนครั้งที่ต้องกดให้ผู้เล่นรู้ เพราะจะเป็นการใบ้ว่าสีต้องห้าม
     * โผล่ในแพทเทิร์นกี่ครั้ง ผู้เล่นต้องจำเองว่าเห็นสีนั้นกี่ตัวแล้วหักออกเอง */
    uint8_t len = Pattern_LengthForDifficulty(ctx->difficulty);
    Pattern_Generate(&ctx->question, len);
    Challenge_BuildExpectedAnswer(&ctx->question, ctx->is_reverse_round,
                                  ctx->no_press_color, &expected);

    /* ---- ประกาศ challenge "ก่อน" นับถอยหลัง แล้วค้างไว้ให้อ่านทัน ---- */
    Challenge_AnnounceRoundInfo(ctx->no_press_color, ctx->is_reverse_round);
    BSP_Delay_ms(ROUND_INFO_HOLD_MS);

    /* ---- นับถอยหลัง 3 2 1 แล้วไฟ LED จึงเริ่มเล่นแพทเทิร์น ---- */
    Challenge_DisplayCountdown(3);
    Pattern_Show(&ctx->question);

    /* ---- รับคำตอบ: เล่นต่อจนครบเสมอ นับจำนวนครั้งที่กดผิด ไม่ตัดจบกลางคัน ---- */
    uint8_t wrong_count = Answer_PlayRound(&ctx->answer, &expected, ctx->no_press_color);

    snprintf(line, sizeof(line), "You got %u wrong.", wrong_count);
    BSP_UART_Print(line);

    /* ---- เฉลย "หลังจบด่านแล้ว" เท่านั้น ----
     * อ่านจาก expected ไม่ใช่ ctx->question เพราะ expected ถูกสร้างโดย
     * Challenge_BuildExpectedAnswer ซึ่งจัดการให้ครบแล้วทั้งสองเงื่อนไข:
     *   Reverse: NO  -> ไล่ตามลำดับไฟปกติ
     *   Reverse: YES -> ไล่ย้อนจากท้ายมาหัว
     * และตัดตำแหน่งสีต้องห้ามออกไปแล้วด้วย
     * ส่วน ctx->question เป็นแพทเทิร์นดิบที่ไม่รู้เรื่องทั้งสองอย่างนี้เลย */
    {
        char dbg[96];
        int  n = snprintf(dbg, sizeof(dbg), "Answer:");
        for (uint8_t i = 0; i < expected.length; i++) {
            n += snprintf(dbg + n, sizeof(dbg) - (unsigned)n, " %s",
                          Pattern_ColorName(expected.data[i]));
        }
        BSP_UART_Print(dbg);
    }

    /* ---- อัปเดตคะแนน/HP ---- */
    if (wrong_count == 0) {
        ctx->accuracy_count++;
    } else {
        HP_Decrement(ctx); /* ด่านนี้มีกดผิดอย่างน้อย 1 ครั้ง -> hp -1 (ไม่สนจำนวนครั้ง) */
        snprintf(line, sizeof(line), "HP -1. You have %u left.", ctx->hp);
        BSP_UART_Print(line);
    }

    if (HP_IsGameOver(ctx)) {
        return ROUND_GAME_OVER;   /* game.c จะพิมพ์ GAME OVER ต่อจากบรรทัด HP ข้างบน */
    }

    if (ctx->round >= TOTAL_ROUNDS) {
        BSP_UART_Print("End");    /* ด่านสุดท้ายและยังไม่ตาย -> ไปหน้าสรุปผล */
        return ROUND_ACCURACY_DONE;
    }

    /* ---- ยังมีด่านถัดไป: ค้างผลไว้ให้อ่านทันก่อนขึ้นด่านใหม่ ---- */
    BSP_Delay_ms(ROUND_RESULT_HOLD_MS);
    return ROUND_CONTINUE;
}
