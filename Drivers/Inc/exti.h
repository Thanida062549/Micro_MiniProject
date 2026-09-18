#ifndef EXTI_H
#define EXTI_H

/* เปิด clock + ผูกปุ่ม Reset (PC13) เข้ากับ EXTI15_10 — เรียกครั้งเดียวจาก board.c */
void EXTI_Init(void);

#endif /* EXTI_H */
