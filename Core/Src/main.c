#include "board.h"
#include "game.h"
#include "led.h"
#include "timer.h"

/*
 * main.c — จุดเริ่มต้นของโปรแกรม (เรียกจาก Reset_Handler ใน startup_stm32f411retx.s)
 *
 *   - Startup_Animation() -> ไฟวิ่ง 1 รอบ บอกว่าบอร์ดเริ่มทำงานแล้ว
 *   - BSP_Init()          -> เปิด driver ทุกตัว (timer, led, button, buzzer, uart, adc, exti)
 *   - Game_Init()         -> ตั้งค่าเริ่มต้นของเกม
 *   - Game_Run()          -> เล่นจบ 1 เกมเต็ม แล้ววนกลับไปหน้าเลือกความยากให้เองอัตโนมัติ
 */

/* ไฟวิ่ง 1 ครั้งตอนบูต ใช้ได้ 2 อย่าง:
 *   1. เป็นสัญญาณให้ผู้เล่นรู้ว่าบอร์ดพร้อมแล้ว (ดูดีตอนเดโม)
 *   2. นับรอบบูตด้วยตา โดยไม่ต้องพึ่ง UART เลย
 *      วิ่ง 1 รอบ = บูตครั้งเดียว | วิ่ง 2 รอบติดกัน = บอร์ดบูตซ้ำ
 *
 * หมายเหตุ: ตอนกด Run ใน CubeIDE ตัว ST-Link จะสั่งรีเซ็ตบอร์ดมากกว่า 1 ครั้ง
 * ระหว่างขั้นตอน flash จึงอาจเห็นไฟวิ่ง 2 รอบเฉพาะตอนนั้น ซึ่งเป็นเรื่องปกติ
 * ของ debugger ไม่ใช่ปัญหาของโปรแกรม เวลาเสียบไฟเล่นจริงจะวิ่งรอบเดียว */
static void Startup_Animation(void)
{
    Timer_Init();   /* ต้องมาก่อน เพราะ BSP_Delay_ms ใช้ SysTick */
    LED_Init();

    for (uint8_t i = 1; i <= 4; i++) {
        BSP_LED_On(i);
        BSP_Delay_ms(150);
    }
    BSP_Delay_ms(300);
    BSP_LED_AllOff();
    BSP_Delay_ms(300);
}

int main(void)
{
    GameContext_t ctx;

    Startup_Animation();

    BSP_Init();
    Game_Init(&ctx);

    while (1) {
        Game_Run(&ctx);
    }
}
