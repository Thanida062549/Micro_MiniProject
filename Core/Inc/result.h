#ifndef RESULT_H
#define RESULT_H

#include "game_types.h"

/* หลังเล่นครบ TOTAL_ROUNDS ด่าน (hp ยังไม่หมด) แสดงผลสรุปรวม */
void Result_ShowAccuracy(uint8_t accuracy_count, uint8_t total_rounds);

/* hp หมดระหว่างทาง */
void Result_ShowGameOver(void);

#endif /* RESULT_H */
