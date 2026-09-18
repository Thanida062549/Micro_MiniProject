#include "board.h"
#include "game.h"

/*
 * main.c — จุดเริ่มต้นของโปรแกรม (เรียกจาก Reset_Handler ใน startup_stm32f411retx.s)
 *
 * ไฟล์นี้ไม่มีตรรกะอะไรเลยนอกจาก "เปิดฮาร์ดแวร์ แล้วปล่อยให้เกมทำงาน"
 * ตรงกับหลักการแยก Application/Driver ที่ใช้มาตลอดโปรเจกต์:
 *   - BSP_Init()  -> เปิด driver ทุกตัว (timer, led, button, buzzer, uart, adc, exti)
 *   - Game_Init() -> ตั้งค่าเริ่มต้นของเกม (state = start)
 *   - Game_Run()  -> เล่นจบ 1 เกมเต็ม แล้ววนกลับไปหน้าเลือกความยากให้เองอัตโนมัติ
 */
int main(void)
{
    GameContext_t ctx;

    BSP_Init();
    Game_Init(&ctx);

    while (1) {
        Game_Run(&ctx);
    }
}
