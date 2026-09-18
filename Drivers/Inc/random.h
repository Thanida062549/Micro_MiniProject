#ifndef RANDOM_H
#define RANDOM_H

#include <stdint.h>

/* ตั้งค่า seed เริ่มต้นของตัวสุ่มเลข — เรียกครั้งเดียวจาก board.c หลังเปิด ADC/Timer แล้ว */
void Random_Seed(uint32_t seed);

/* สุ่มเลขจำนวนเต็มในช่วง [min_val, max_val] (xorshift32) */
uint8_t BSP_Random_Range(uint8_t min_val, uint8_t max_val);

#endif /* RANDOM_H */
