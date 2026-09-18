#include "random.h"

/* =====================================================================
 * Random driver — xorshift32 (เบา เร็ว เหมาะกับ MCU)
 * seed ผสมจาก ADC noise + tick เวลาตอนบูต (ดูใน board.c)
 * ===================================================================== */

static uint32_t rng_state = 0;

void Random_Seed(uint32_t seed)
{
    rng_state = seed | 1UL; /* seed ต้องไม่เป็น 0 */
}

uint8_t BSP_Random_Range(uint8_t min_val, uint8_t max_val)
{
    if (rng_state == 0) {
        rng_state = 1UL; /* fallback เผื่อไม่มีใครเรียก Random_Seed มาก่อน */
    }
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;

    return (uint8_t)(min_val + (rng_state % (uint32_t)(max_val - min_val + 1)));
}
