#ifndef MEMORY_GAME_H
#define MEMORY_GAME_H

#include "game_types.h"

/* ตั้งค่าเริ่มต้นใหม่ทั้งเกม (state: start -> round=0, hp=3) */
void MemoryGame_ResetForNewGame(GameContext_t *ctx);

/* เล่น 1 ด่านเต็มๆ ตั้งแต่ select_level จนถึง check_answer และแสดงผลด่านนั้น
 * คืนค่าว่าเกมควรไปต่อ, จบแบบครบด่าน, หรือจบแบบ hp หมด */
RoundOutcome_t MemoryGame_PlayRound(GameContext_t *ctx);

#endif /* MEMORY_GAME_H */
