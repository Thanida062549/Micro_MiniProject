#include "game.h"
#include "memory_game.h"
#include "result.h"

/*
 * วิธีใช้งานใน main.c (ตัวอย่างการเรียกใช้ ไม่รวมอยู่ในไฟล์นี้):
 *
 *   #include "board.h"
 *   #include "game.h"
 *
 *   GameContext_t ctx;
 *   BSP_Init();           // เปิด driver ทุกตัว (timer, led, button, buzzer, uart, adc, exti)
 *   Game_Init(&ctx);
 *   while (1) {
 *       Game_Run(&ctx);   // เล่นจบ 1 เกมเต็ม แล้ววนกลับไปหน้าเลือกความยากให้เอง
 *   }
 */

void Game_Init(GameContext_t *ctx)
{
    /* ---- state: start ---- */
    ctx->state = STATE_START;
    MemoryGame_ResetForNewGame(ctx);   /* รวมหน้าเลือกความยาก + Round 1 + countdown ไว้ในนี้แล้ว */
    ctx->state = STATE_MEMORY_GAME;
}

void Game_Run(GameContext_t *ctx)
{
    RoundOutcome_t outcome;

    do {
        outcome = MemoryGame_PlayRound(ctx);
    } while (outcome == ROUND_CONTINUE);

    if (outcome == ROUND_GAME_OVER) {
        ctx->state = STATE_GAME_OVER;
        Result_ShowGameOver();
    } else {
        ctx->state = STATE_DISPLAY_ACCURACY;
        Result_ShowAccuracy(ctx->accuracy_count, TOTAL_ROUNDS);
    }

    /* กลับไปเริ่มเกมใหม่ (state -> start): Game_Init จะพาไปหน้าเลือกความยากทันที */
    Game_Init(ctx);
}
