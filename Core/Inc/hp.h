#ifndef HP_H
#define HP_H

#include "game_types.h"

/* ตั้งค่า hp เริ่มต้น (state: start -> hp = 3) */
void HP_Init(GameContext_t *ctx);

/* ลด hp ลง 1 เมื่อตอบผิด (state: display_incorrect_hp -> hp = hp - 1) */
void HP_Decrement(GameContext_t *ctx);

/* ตรวจว่า hp หมดแล้วหรือยัง */
uint8_t HP_IsGameOver(const GameContext_t *ctx);

#endif /* HP_H */
