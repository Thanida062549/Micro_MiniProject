#include "adc.h"
#include "timer.h"

/* =====================================================================
 * ADC driver สำหรับ STM32F411RE
 * Pin mapping: Potentiometer = PA4 (ADC1_IN4)
 * ===================================================================== */

#define RCC_BASE      0x40023800UL
#define GPIOA_BASE    0x40020000UL
#define ADC1_BASE     0x40012000UL

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

#define ADC_SR_EOC      (1UL << 1)
#define ADC_CR1_EOCIE   (1UL << 5)
#define ADC_CR2_ADON    (1UL << 0)
#define ADC_CR2_SWSTART (1UL << 30)

#define NVIC_ISER0     (*(volatile uint32_t *)0xE000E100UL)  /* IRQ 0-31 */
#define ADC_IRQn        18

static volatile uint16_t adc_value = 0;
static volatile uint8_t  adc_ready = 0;

void ADC_Init(void)
{
    RCC_AHB1ENR |= (1UL << 0);  /* GPIOA */
    RCC_APB2ENR |= (1UL << 8);  /* ADC1 */

    /* PA4: analog mode (ต้องปิด pull ด้วยตอน analog) */
    GPIO_MODER(GPIOA_BASE) &= ~(3UL << (4 * 2));
    GPIO_MODER(GPIOA_BASE) |=  (3UL << (4 * 2));  /* 11 = analog */
    GPIO_PUPDR(GPIOA_BASE) &= ~(3UL << (4 * 2));

    /* single conversion บนช่อง 4 (PA4), sample time ยาวเพื่อความนิ่ง */
    ADC1_SMPR2 |= (7UL << 12);   /* channel4: sample time 480 cycles */
    ADC1_SQR3   = 4;             /* ลำดับการแปลงที่ 1 = channel 4 */
    ADC1_CR1   |= ADC_CR1_EOCIE; /* interrupt เมื่อแปลงเสร็จ (ห้าม polling) */
    ADC1_CR2   |= ADC_CR2_ADON;  /* เปิด ADC */
    NVIC_ISER0  = (1UL << ADC_IRQn);

    BSP_Delay_ms(1); /* รอ ADC stabilize ตามสเปกก่อนแปลงครั้งแรก */
}

/* ชื่อฟังก์ชันต้องตรงกับ vector table ใน startup_stm32f411retx.s */
void ADC_IRQHandler(void)
{
    if (ADC1_SR & ADC_SR_EOC) {
        adc_value = (uint16_t)ADC1_DR;   /* อ่าน DR จะเคลียร์ EOC ให้อัตโนมัติ */
        adc_ready = 1;
    }
}

uint16_t ADC_ReadRaw(void)
{
    adc_ready = 0;
    ADC1_CR2 |= ADC_CR2_SWSTART;
    while (!adc_ready) {
        /* รอ flag ที่ ADC_IRQHandler ตั้งให้ ไม่ใช่การ polling ฮาร์ดแวร์ */
    }
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
