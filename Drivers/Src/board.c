#include "board.h"
#include "timer.h"
#include "led.h"
#include "button.h"
#include "buzzer.h"
#include "uart.h"
#include "adc.h"
#include "exti.h"
#include "random.h"

/*
 * board.c คือจุดเดียวที่รู้จัก driver ทุกตัวพร้อมกัน — Game Logic ทั้ง 7 ไฟล์
 * ไม่ต้อง include ไฟล์นี้เลย เพราะแต่ละไฟล์ include เฉพาะ driver ที่ตัวเองใช้จริง
 * (เช่น pattern.c include แค่ led.h/timer.h/random.h ไม่รู้จัก uart.h เลย)
 */
void BSP_Init(void)
{
    Timer_Init();
    Button_Init();
    LED_Init();
    Buzzer_Init();
    UART_Init();
    ADC_Init();

    EXTI_Init();

    Random_Seed(((uint32_t)ADC_ReadRaw() << 16) | Timer_GetTick());
}
