#ifndef LED_H
#define LED_H

#include <stdint.h>

/* เปิด clock + ตั้งค่า GPIO ของ LED ทั้ง 4 ดวงเป็น output — เรียกครั้งเดียวจาก board.c */
void LED_Init(void);

void BSP_LED_On(uint8_t led_index);   /* led_index: 1-4 */
void BSP_LED_Off(uint8_t led_index);
void BSP_LED_AllOff(void);

#endif /* LED_H */
