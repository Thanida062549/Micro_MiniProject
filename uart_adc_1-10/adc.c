#include "adc.h"
#include "timer.h"

/* =====================================================================
 * ADC driver สำหรับ STM32F411RE
 * Pin mapping: Potentiometer = PA4 (ADC1_IN4)
 *
 * แปลงต่อเนื่องอยู่เบื้องหลังแบบไม่มีการรอเลย:
 *   ADC แปลงค่าซ้ำเองไม่หยุด (continuous) ทุกครั้งที่แปลงเสร็จจะแจ้ง interrupt
 *   ADC_IRQHandler() เก็บค่าล่าสุดไว้ในตัวแปร ฟังก์ชันอ่านค่าจึงแค่คืนค่าที่เก็บไว้
 *   ทันที ไม่ต้องสั่งแปลงและไม่ต้องวนรอผล
 * ===================================================================== */

#define RCC_BASE      0x40023800UL
#define GPIOA_BASE    0x40020000UL
#define ADC1_BASE     0x40012000UL
#define ADC_COMMON    0x40012300UL

#define RCC_AHB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR   (*(volatile uint32_t *)(RCC_BASE + 0x44))

#define GPIO_MODER(base)   (*(volatile uint32_t *)((base) + 0x00))
#define GPIO_PUPDR(base)   (*(volatile uint32_t *)((base) + 0x0C))

#define ADC1_SR      (*(volatile uint32_t *)(ADC1_BASE + 0x00))
#define ADC1_CR1     (*(volatile uint32_t *)(ADC1_BASE + 0x04))
#define ADC1_CR2     (*(volatile uint32_t *)(ADC1_BASE + 0x08))
#define ADC1_SMPR2   (*(volatile uint32_t *)(ADC1_BASE + 0x10))
#define ADC1_SQR3    (*(volatile uint32_t *)(ADC1_BASE + 0x34))
#define ADC1_DR      (*(volatile uint32_t *)(ADC1_BASE + 0x4C))
#define ADC_CCR      (*(volatile uint32_t *)(ADC_COMMON + 0x04))

#define ADC_SR_OVR      (1UL << 5)
#define ADC_SR_EOC      (1UL << 1)
#define ADC_CR1_EOCIE   (1UL << 5)
#define ADC_CR2_CONT    (1UL << 1)
#define ADC_CR2_ADON    (1UL << 0)
#define ADC_CR2_SWSTART (1UL << 30)
#define ADC_CCR_ADCPRE_MASK  (3UL << 16)
#define ADC_CCR_ADCPRE_DIV8  (3UL << 16)

#define NVIC_ISER0     (*(volatile uint32_t *)0xE000E100UL)  /* IRQ 0-31 */
#define ADC_IRQn        18

static volatile uint16_t adc_value = 0;   /* ค่าล่าสุด เขียนโดย ISR อ่านโดยโปรแกรมหลัก */

void ADC_Init(void)
{
    RCC_AHB1ENR |= (1UL << 0);  /* GPIOA */
    RCC_APB2ENR |= (1UL << 8);  /* ADC1 */

    /* PA4: analog mode (ต้องปิด pull ด้วยตอน analog) */
    GPIO_MODER(GPIOA_BASE) &= ~(3UL << (4 * 2));
    GPIO_MODER(GPIOA_BASE) |=  (3UL << (4 * 2));  /* 11 = analog */
    GPIO_PUPDR(GPIOA_BASE) &= ~(3UL << (4 * 2));

    /* ลดความเร็ว clock ของ ADC ลงเหลือ 1/8 ของ PCLK2 (16 MHz -> 2 MHz)
     * เพราะแปลงต่อเนื่องแล้วทุกครั้งที่เสร็จจะเข้า ISR การแปลงช้าลงทำให้ ISR ถูกเรียก
     * น้อยลง (ราว 4 พันครั้งต่อวินาที) กิน CPU ประมาณ 1% ค่า potentiometer ไม่ได้
     * เปลี่ยนเร็วอยู่แล้ว ไม่ต้องการอัตราสุ่มสูง */
    ADC_CCR = (ADC_CCR & ~ADC_CCR_ADCPRE_MASK) | ADC_CCR_ADCPRE_DIV8;

    /* ช่อง 4 (PA4), sample time ยาวสุดเพื่อความนิ่ง, แปลงต่อเนื่อง, แจ้ง interrupt ทุกครั้งที่แปลงเสร็จ */
    ADC1_SMPR2 |= (7UL << 12);   /* channel4: sample time 480 cycles */
    ADC1_SQR3   = 4;             /* ลำดับการแปลงที่ 1 = channel 4 */
    ADC1_CR1   |= ADC_CR1_EOCIE;
    ADC1_CR2   |= ADC_CR2_CONT | ADC_CR2_ADON;
    NVIC_ISER0  = (1UL << ADC_IRQn);

    BSP_Delay_ms(1);             /* รอ ADC stabilize ตามสเปก (เป็นการหน่วงเวลา ไม่ใช่การรอผลจาก ADC) */

    ADC1_CR2 |= ADC_CR2_SWSTART; /* สั่งเริ่มครั้งเดียว หลังจากนี้แปลงซ้ำเองเรื่อยๆ */

    BSP_Delay_ms(2);             /* หน่วงเวลาคงที่ให้แปลงเสร็จอย่างน้อย 1 รอบ (ใช้เวลาราว 0.25 ms)
                                    จะได้มีค่าจริงไว้ให้ ADC_ReadRaw() ตั้งแต่ครั้งแรกที่เรียก */
}

/* ชื่อฟังก์ชันต้องตรงกับ vector table ใน startup_stm32f411retx.s */
void ADC_IRQHandler(void)
{
    if (ADC1_SR & ADC_SR_EOC) {
        adc_value = (uint16_t)ADC1_DR;   /* อ่าน DR จะเคลียร์ EOC ให้อัตโนมัติ */
    }
    if (ADC1_SR & ADC_SR_OVR) {
        ADC1_SR &= ~ADC_SR_OVR;          /* กันไว้เผื่อ ISR ตอบสนองช้าจนเกิด overrun */
    }
}

/* คืนค่าล่าสุดที่ ISR เก็บไว้ทันที ไม่มีการรอ */
uint16_t ADC_ReadRaw(void)
{
    return adc_value;
}

uint8_t BSP_ReadDifficulty(void)
{
    /* สลับทิศจากเดิม: ของเดิมค่า ADC ต่ำ = EASY ทำให้หมุนตามเข็มสุดได้ EASY
     * ซึ่งกลับด้านกับที่ต้องการ จึงสลับให้ค่า ADC ต่ำ = HARD แทน
     * ผลลัพธ์: ทวนเข็มจนสุด = EASY, กลาง = MEDIUM, ตามเข็มจนสุด = HARD */
    uint16_t raw = ADC_ReadRaw();   /* 0-4095 (12-bit) */
    if (raw < 1365) {
        return 2;   /* HARD: โซนค่า ADC ต่ำ (หมุนตามเข็มจนสุด) */
    }
    if (raw < 2730) {
        return 1;   /* MEDIUM: โซนกลาง */
    }
    return 0;       /* EASY: โซนค่า ADC สูง (หมุนทวนเข็มจนสุด) */
}
