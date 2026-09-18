#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* เปิด SysTick ให้ tick ทุก 1ms — เรียกครั้งเดียวจาก board.c ก่อนไฟล์อื่นใช้ Delay/GetTick */
void Timer_Init(void);

/* บล็อกรอ ms มิลลิวินาที (นับจาก SysTick interrupt ไม่ใช่ busy-wait อ่านฮาร์ดแวร์ตรงๆ) */
void BSP_Delay_ms(uint32_t ms);

/* คืนค่าตัวนับ millisecond ปัจจุบัน (ใช้เป็น entropy สำหรับ seed สุ่มเลขใน random.c) */
uint32_t Timer_GetTick(void);

#endif /* TIMER_H */
