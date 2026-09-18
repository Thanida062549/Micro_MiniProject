#ifndef GAME_H
#define GAME_H

#include "game_types.h"

/* เตรียมเกมใหม่ทั้งหมด (state: start) เรียกครั้งเดียวตอนบูต */
void Game_Init(GameContext_t *ctx);

/* เล่นเกม 1 รอบเต็ม (start -> ... -> accuracy/game_over) แล้ววนกลับไป start ให้อัตโนมัติ
 * เรียกซ้ำได้เรื่อยๆ ใน main loop */
void Game_Run(GameContext_t *ctx);

#endif /* GAME_H */
