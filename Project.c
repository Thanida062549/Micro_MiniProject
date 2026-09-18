/*
 * Pattern Memory Master Game
 * STM32F411RE - CMSIS / register level, no HAL
 *
 * HARDWARE MAP
 *   LED Blue       PA5       Button Blue       PA10
 *   LED Red        PA6       Button Red        PB3
 *   LED Yellow     PA7       Button Yellow     PB5
 *   LED Green      PB6       Button Green      PB4
 *   Potentiometer  PA4       ADC1_IN4
 *   Submit         PC13      input pull-up
 *   Reset          PC14      EXTI14, falling edge
 *   Buzzer         PB0       TIM3_CH3 / AF2
 *   UART           PA2/PA3   USART2 -> ST-LINK Virtual COM Port
 *
 * Serial terminal: 115200 baud, 8 data bits, no parity, 1 stop bit.
 */

#include "stm32f411xe.h"
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define SYSCLK_HZ  84000000UL
#define APB1CLK_HZ 42000000UL
#define MAX_ROUND       5U
#define START_HP        3U
#define MAX_PATTERN     10U
#define C_NUM           4U
#define C_NONE          0xFFU

/* ----------------------------- Colors ----------------------------- */
typedef enum {
    C_BLUE = 0,
    C_RED,
    C_YELLOW,
    C_GREEN
} Color;

static const char *COLOR_NAME[C_NUM] = {
    "BLUE", "RED", "YELLOW", "GREEN"
};

static const uint16_t COLOR_TONE[C_NUM] = {
    523, 659, 784, 988
};

/* ----------------------------- Levels ----------------------------- */
typedef enum {
    LV_SIMPLE = 0,
    LV_MEDIUM,
    LV_HARD
} Level;

typedef struct {
    const char *name;
    uint8_t pattern_length;
    uint16_t led_on_ms;
    uint16_t led_off_ms;
} LevelCfg;

static const LevelCfg LEVEL_TABLE[3] = {
    { "SIMPLE", 5, 700, 300 },
    { "MEDIUM", 8, 450, 200 },
    { "HARD",  10, 250, 120 }
};

/* ----------------------------- State ----------------------------- */
typedef enum {
    STATE_START = 0,
    STATE_SELECT_LEVEL,
    STATE_INIT_ROUND,
    STATE_RANDOM_NO_PRESS,
    STATE_COUNTDOWN,
    STATE_STORE_QUESTION_PATTERN,
    STATE_SHOW_PATTERN,
    STATE_RANDOM_BUZZ,
    STATE_COLLECT_ANSWER,
    STATE_CHECK_ANSWER,
    STATE_DISPLAY_CORRECT,
    STATE_DISPLAY_INCORRECT,
    STATE_CHECK_HP,
    STATE_CHECK_ROUND,
    STATE_GAME_OVER,
    STATE_DISPLAY_ACCURACY
} GameState;

/* ----------------------------- Hardware ----------------------------- */
static GPIO_TypeDef * const LED_PORT[C_NUM] = {
    GPIOA, GPIOA, GPIOA, GPIOB
};
static const uint8_t LED_BIT[C_NUM] = { 5, 6, 7, 6 };

static GPIO_TypeDef * const BTN_PORT[C_NUM] = {
    GPIOA, GPIOB, GPIOB, GPIOB
};
static const uint8_t BTN_BIT[C_NUM] = { 10, 3, 5, 4 };

#define SUBMIT_PORT GPIOC
#define SUBMIT_BIT  13U
#define RESET_PORT  GPIOC
#define RESET_BIT   14U
#define BUZZ_PORT   GPIOB
#define BUZZ_BIT    0U

/* ----------------------------- Globals ----------------------------- */
volatile uint32_t ms_ticks = 0;
volatile uint8_t reset_flag = 0;
volatile uint8_t submit_irq_flag = 0;

static uint32_t reset_last_tick = 0;
static uint32_t submit_last_tick = 0;

static Level game_level = LV_SIMPLE;
static uint8_t hp = START_HP;
static uint8_t round_no = 0;
static uint8_t accuracy_count = 0;
static uint8_t pattern_length = 0;

static uint8_t Store_Question_Pattern[MAX_PATTERN];
static uint8_t Store_Reverse_Pattern[MAX_PATTERN];
static uint8_t Store_Check_Pattern[MAX_PATTERN];
static uint8_t Store_Answer_Pattern[MAX_PATTERN];

static uint8_t check_pattern_count = 0;
static uint8_t answer_count = 0;

static uint8_t random_no_press = C_NONE;
static uint8_t random_buzz = 0;
static uint8_t reverse_enable = 0;

/* ----------------------------- Prototypes ----------------------------- */
static void SystemClock_Config(void);
static void SysTick_Init(void);
static void GPIO_Init_All(void);
static void ADC1_Init(void);
static void TIM3_PWM_Init(void);
static void USART2_Init(void);
static void EXTI14_Reset_Init(void);

static void uart_putc(char c);
static void uprint(const char *fmt, ...);

static void led_all_off(void);
static void led_on(uint8_t color);
static void buzzer_on(uint16_t freq);
static void buzzer_off(void);
static void delay_ms(uint32_t ms);

static uint16_t adc_read(void);
static Level read_level_from_pot(void);
static uint8_t button_down(uint8_t color);
static uint8_t submit_down(void);
static uint8_t button_pressed(uint8_t color);
static uint8_t submit_pressed(void);
static uint8_t consume_submit_irq(void);

static void clear_patterns(void);
static void reset_game_data(void);
static void wait_release_color(uint8_t color);
static void wait_release_submit(void);

static void display_start(void);
static void display_select_level(Level lv, uint16_t adc);
static void display_dont_press(void);
static void display_pattern(void);
static void display_correct(void);
static void display_incorrect(void);
static void display_game_over(void);
static void display_accuracy(void);

static GameState state_start(void);
static GameState state_select_level(void);
static GameState state_init_round(void);
static GameState state_random_no_press(void);
static GameState state_countdown(void);
static GameState state_store_question_pattern(void);
static GameState state_show_pattern(void);
static GameState state_random_buzz(void);
static GameState state_collect_answer(void);
static GameState state_check_answer(void);
static GameState state_display_correct(void);
static GameState state_display_incorrect(void);
static GameState state_check_hp(void);
static GameState state_check_round(void);
static GameState state_game_over(void);
static GameState state_display_accuracy(void);
static GameState state_run(GameState s);

/* ===================================================================== */
/* MAIN                                                                  */
/* ===================================================================== */
int main(void)
{
    SystemClock_Config();
    SysTick_Init();
    GPIO_Init_All();
    ADC1_Init();
    TIM3_PWM_Init();
    USART2_Init();
    EXTI14_Reset_Init();

    led_all_off();
    buzzer_off();

    uprint("\r\n\r\n");
    uprint("UART OK - Pattern Memory Master Game\r\n");
    uprint("USART2 115200 8N1\r\n");

    GameState state = STATE_START;

    while (1)
    {
        if (reset_flag)
        {
            reset_flag = 0;
            submit_irq_flag = 0;
            led_all_off();
            buzzer_off();
            reset_game_data();
            uprint("\r\n[RESET INTERRUPT]\r\n");
            state = STATE_START;
        }

        state = state_run(state);
    }
}

static GameState state_run(GameState s)
{
    switch (s)
    {
        case STATE_START:                   return state_start();
        case STATE_SELECT_LEVEL:            return state_select_level();
        case STATE_INIT_ROUND:              return state_init_round();
        case STATE_RANDOM_NO_PRESS:         return state_random_no_press();
        case STATE_COUNTDOWN:               return state_countdown();
        case STATE_STORE_QUESTION_PATTERN:  return state_store_question_pattern();
        case STATE_SHOW_PATTERN:            return state_show_pattern();
        case STATE_RANDOM_BUZZ:             return state_random_buzz();
        case STATE_COLLECT_ANSWER:          return state_collect_answer();
        case STATE_CHECK_ANSWER:            return state_check_answer();
        case STATE_DISPLAY_CORRECT:         return state_display_correct();
        case STATE_DISPLAY_INCORRECT:       return state_display_incorrect();
        case STATE_CHECK_HP:                return state_check_hp();
        case STATE_CHECK_ROUND:             return state_check_round();
        case STATE_GAME_OVER:               return state_game_over();
        case STATE_DISPLAY_ACCURACY:        return state_display_accuracy();
        default:                            return STATE_START;
    }
}

/* ===================================================================== */
/* GAME STATES                                                           */
/* ===================================================================== */
static GameState state_start(void)
{
    display_start();
    led_all_off();
    buzzer_off();

    while (!reset_flag)
    {
        if (submit_pressed())
        {
            wait_release_submit();
            srand((unsigned int)ms_ticks);
            uprint("\r\n[START] -> SELECT LEVEL\r\n");
            return STATE_SELECT_LEVEL;
        }
        delay_ms(10);
    }
    return STATE_START;
}

static GameState state_select_level(void)
{
    uint8_t old_level = 0xFF;
    uint16_t adc = 0;

    uprint("\r\n==========================================\r\n");
    uprint("             SELECT LEVEL\r\n");
    uprint("  Turn potentiometer and press SUBMIT\r\n");
    uprint("==========================================\r\n");

    while (!reset_flag)
    {
        adc = adc_read();
        game_level = read_level_from_pot();

        if ((uint8_t)game_level != old_level)
        {
            old_level = (uint8_t)game_level;
            display_select_level(game_level, adc);
        }

        if (submit_pressed())
        {
            wait_release_submit();
            uprint("\r\n[LEVEL LOCKED] %s\r\n", LEVEL_TABLE[game_level].name);
            return STATE_INIT_ROUND;
        }
        delay_ms(20);
    }
    return STATE_START;
}

static GameState state_init_round(void)
{
    if (round_no == 0)
    {
        hp = START_HP;
        accuracy_count = 0;
    }

    pattern_length = LEVEL_TABLE[game_level].pattern_length;
    clear_patterns();

    uprint("\r\n========== ROUND %u / %u ==========\r\n", round_no + 1U, MAX_ROUND);
    uprint("Level: %s    HP: %u\r\n", LEVEL_TABLE[game_level].name, hp);

    return STATE_RANDOM_NO_PRESS;
}

static GameState state_random_no_press(void)
{
    if ((rand() % 2) == 0)
        random_no_press = C_NONE;
    else
        random_no_press = (uint8_t)(rand() % C_NUM);

    display_dont_press();
    return STATE_COUNTDOWN;
}

static GameState state_countdown(void)
{
    for (int n = 3; n >= 1; n--)
    {
        uprint("[COUNTDOWN] %d\r\n", n);
        buzzer_on(500 + (uint16_t)n * 100U);
        led_all_off();
        delay_ms(120);
        buzzer_off();
        delay_ms(880);
    }

    uprint("[GO]\r\n");
    return STATE_STORE_QUESTION_PATTERN;
}

static GameState state_store_question_pattern(void)
{
    clear_patterns();

    for (uint8_t i = 0; i < pattern_length; i++)
    {
        uint8_t c;
        do {
            c = (uint8_t)(rand() % C_NUM);
        } while (c == random_no_press);

        Store_Question_Pattern[i] = c;
    }

    for (uint8_t i = 0; i < pattern_length; i++)
    {
        Store_Reverse_Pattern[i] =
            Store_Question_Pattern[pattern_length - 1U - i];
    }

    uprint("[Store_Question_Pattern] %u items\r\n", pattern_length);
    return STATE_SHOW_PATTERN;
}

static GameState state_show_pattern(void)
{
    display_pattern();
    uprint("\r\n[SHOW PATTERN] Watch carefully...\r\n");

    for (uint8_t i = 0; i < pattern_length; i++)
    {
        uint8_t c = Store_Question_Pattern[i];
        uint16_t on_ms = LEVEL_TABLE[game_level].led_on_ms;
        uint16_t off_ms = LEVEL_TABLE[game_level].led_off_ms;

        led_all_off();
        led_on(c);
        buzzer_on(COLOR_TONE[c]);
        delay_ms(on_ms);
        buzzer_off();
        led_all_off();
        delay_ms(off_ms);
    }

    return STATE_RANDOM_BUZZ;
}

static GameState state_random_buzz(void)
{
    random_buzz = (uint8_t)(rand() % 2);
    reverse_enable = 0;

    if (random_buzz)
    {
        reverse_enable = 1;
        memcpy(Store_Check_Pattern, Store_Reverse_Pattern, pattern_length);
        check_pattern_count = pattern_length;

        uprint("\r\n[RANDOM BUZZ] = 1\r\n");
        uprint("[REVERSE PATTERN] = ON\r\n");
        uprint("Answer BACKWARD: ");
        for (uint8_t i = 0; i < check_pattern_count; i++)
            uprint("%s%s", COLOR_NAME[Store_Check_Pattern[i]],
                   (i + 1U < check_pattern_count) ? " -> " : "");
        uprint("\r\n");

        buzzer_on(1500);
        delay_ms(180);
        buzzer_off();
    }
    else
    {
        memcpy(Store_Check_Pattern, Store_Question_Pattern, pattern_length);
        check_pattern_count = pattern_length;
        uprint("\r\n[RANDOM BUZZ] = 0\r\n");
        uprint("[NORMAL ORDER]\r\n");
    }

    answer_count = 0;
    memset(Store_Answer_Pattern, 0, sizeof(Store_Answer_Pattern));
    return STATE_COLLECT_ANSWER;
}

static GameState state_collect_answer(void)
{
    uprint("\r\n[ans_press_collect]\r\n");
    uprint("Press colors, then SUBMIT.\r\n");

    answer_count = 0;

    while (!reset_flag)
    {
        for (uint8_t c = 0; c < C_NUM; c++)
        {
            if (button_pressed(c))
            {
                wait_release_color(c);

                if (c == random_no_press)
                {
                    uprint("[DON'T PRESS VIOLATION] %s\r\n", COLOR_NAME[c]);
                    return STATE_DISPLAY_INCORRECT;
                }

                if (answer_count < MAX_PATTERN)
                {
                    Store_Answer_Pattern[answer_count++] = c;
                    uprint("[%u] %s\r\n", answer_count, COLOR_NAME[c]);
                }

                led_all_off();
                led_on(c);
                buzzer_on(COLOR_TONE[c]);
                delay_ms(80);
                buzzer_off();
                led_all_off();
            }
        }

        if (submit_pressed())
        {
            wait_release_submit();
            uprint("[Submit_Button] = 1\r\n");
            uprint("[Store_Answer_Pattern] count = %u\r\n", answer_count);
            return STATE_CHECK_ANSWER;
        }

        delay_ms(5);
    }

    return STATE_START;
}

static GameState state_check_answer(void)
{
    uint8_t correct = 1;

    if (answer_count != check_pattern_count)
        correct = 0;
    else
    {
        for (uint8_t i = 0; i < check_pattern_count; i++)
        {
            if (Store_Answer_Pattern[i] != Store_Check_Pattern[i])
            {
                correct = 0;
                break;
            }
        }
    }

    uprint("[check_answer] = %u\r\n", correct);
    return correct ? STATE_DISPLAY_CORRECT : STATE_DISPLAY_INCORRECT;
}

static GameState state_display_correct(void)
{
    accuracy_count++;
    display_correct();

    led_all_off();
    for (uint8_t c = 0; c < C_NUM; c++)
    {
        led_on(c);
        buzzer_on(900 + c * 150U);
        delay_ms(90);
        buzzer_off();
        led_all_off();
        delay_ms(40);
    }

    return STATE_CHECK_ROUND;
}

static GameState state_display_incorrect(void)
{
    if (hp > 0)
        hp--;

    display_incorrect();

    led_all_off();
    for (uint8_t k = 0; k < 2; k++)
    {
        led_on(C_RED);
        buzzer_on(200);
        delay_ms(200);
        buzzer_off();
        led_all_off();
        delay_ms(150);
    }

    return STATE_CHECK_HP;
}

static GameState state_check_hp(void)
{
    if (hp == 0)
    {
        return STATE_GAME_OVER;
    }

    return STATE_CHECK_ROUND;
}

static GameState state_check_round(void)
{
    round_no++;

    if (round_no < MAX_ROUND)
        return STATE_INIT_ROUND;

    return STATE_DISPLAY_ACCURACY;
}

static GameState state_game_over(void)
{
    display_game_over();

    for (uint8_t k = 0; k < 3; k++)
    {
        for (uint8_t c = 0; c < C_NUM; c++)
            led_on(c);
        buzzer_on(150);
        delay_ms(250);
        buzzer_off();
        led_all_off();
        delay_ms(180);
    }

    while (!reset_flag)
    {
        if (submit_pressed())
        {
            wait_release_submit();
            reset_game_data();
            uprint("\r\n[SUBMIT] Back to START\r\n");
            return STATE_START;
        }
        delay_ms(20);
    }

    return STATE_START;
}

static GameState state_display_accuracy(void)
{
    display_accuracy();
    led_all_off();

    while (!reset_flag)
    {
        if (submit_pressed())
        {
            wait_release_submit();
            reset_game_data();
            uprint("\r\n[SUBMIT] Back to START\r\n");
            return STATE_START;
        }
        delay_ms(20);
    }

    return STATE_START;
}

/* ===================================================================== */
/* DISPLAY                                                               */
/* ===================================================================== */
static void display_start(void)
{
    uprint("\r\n==========================================\r\n");
    uprint(" Welcome to Pattern Memory Master Game\r\n");
    uprint("==========================================\r\n");
    uprint("Press SUBMIT to start\r\n");
    uprint("RESET works anytime.\r\n\r\n");
}

static void display_select_level(Level lv, uint16_t adc)
{
    uprint("[DISPLAY SELECT LEVEL]\r\n");
    uprint("ADC = %u  ->  LEVEL = %s\r\n", adc, LEVEL_TABLE[lv].name);
    uprint("Simple: 0-1364 | Medium: 1365-2729 | Hard: 2730-4095\r\n");
}

static void display_dont_press(void)
{
    if (random_no_press == C_NONE)
        uprint("[DON'T PRESS] None this round.\r\n");
    else
        uprint("[DON'T PRESS] COLOR = %s\r\n", COLOR_NAME[random_no_press]);
}

static void display_pattern(void)
{
    uprint("[QUESTION PATTERN] ");
    for (uint8_t i = 0; i < pattern_length; i++)
    {
        uprint("%s%s", COLOR_NAME[Store_Question_Pattern[i]],
               (i + 1U < pattern_length) ? " -> " : "");
    }
    uprint("\r\n");
}

static void display_correct(void)
{
    uprint("\r\n******** DISPLAY CORRECT ********\r\n");
    uprint("Round %u / %u : CORRECT\r\n", round_no + 1U, MAX_ROUND);
    uprint("Accuracy count = %u\r\n", accuracy_count);
    uprint("HP = %u\r\n", hp);
    uprint("**********************************\r\n");
}

static void display_incorrect(void)
{
    uprint("\r\n******** DISPLAY INCORRECT ********\r\n");
    uprint("Round %u / %u : INCORRECT\r\n", round_no + 1U, MAX_ROUND);
    uprint("Correct answer: ");
    for (uint8_t i = 0; i < check_pattern_count; i++)
        uprint("%s%s", COLOR_NAME[Store_Check_Pattern[i]],
               (i + 1U < check_pattern_count) ? " -> " : "");
    uprint("\r\n");
    uprint("DISPLAY HP: %u -> %u\r\n", hp + 1U, hp);
    uprint("************************************\r\n");
}

static void display_game_over(void)
{
    uprint("\r\n##########################################\r\n");
    uprint("              GAME OVER\r\n");
    uprint("HP = 0\r\n");
    uprint("Rounds completed: %u / %u\r\n", round_no, MAX_ROUND);
    uprint("Correct rounds: %u\r\n", accuracy_count);
    uprint("Accuracy is NOT displayed because HP reached 0.\r\n");
    uprint("Press SUBMIT for a new game or RESET.\r\n");
    uprint("##########################################\r\n");
}

static void display_accuracy(void)
{
    uint32_t percent = ((uint32_t)accuracy_count * 100U) / MAX_ROUND;

    uprint("\r\n==========================================\r\n");
    uprint("              DISPLAY ACCURACY\r\n");
    uprint("==========================================\r\n");
    uprint("Level          : %s\r\n", LEVEL_TABLE[game_level].name);
    uprint("HP remaining   : %u / %u\r\n", hp, START_HP);
    uprint("Correct rounds : %u / %u\r\n", accuracy_count, MAX_ROUND);
    uprint("Accuracy       : %lu %%\r\n", (unsigned long)percent);
    uprint("Press SUBMIT for a new game or RESET.\r\n");
    uprint("==========================================\r\n");
}

/* ===================================================================== */
/* UTILITY                                                               */
/* ===================================================================== */
static void clear_patterns(void)
{
    memset(Store_Question_Pattern, 0, sizeof(Store_Question_Pattern));
    memset(Store_Reverse_Pattern, 0, sizeof(Store_Reverse_Pattern));
    memset(Store_Check_Pattern, 0, sizeof(Store_Check_Pattern));
    memset(Store_Answer_Pattern, 0, sizeof(Store_Answer_Pattern));
    check_pattern_count = 0;
    answer_count = 0;
}

static void reset_game_data(void)
{
    hp = START_HP;
    round_no = 0;
    accuracy_count = 0;
    pattern_length = 0;
    random_no_press = C_NONE;
    random_buzz = 0;
    reverse_enable = 0;
    clear_patterns();
    led_all_off();
    buzzer_off();
}

static void led_all_off(void)
{
    for (uint8_t c = 0; c < C_NUM; c++)
        LED_PORT[c]->BSRR = (1UL << (LED_BIT[c] + 16U));
}

static void led_on(uint8_t color)
{
    if (color < C_NUM)
        LED_PORT[color]->BSRR = (1UL << LED_BIT[color]);
}

static void buzzer_on(uint16_t freq)
{
    if (freq == 0) return;

    uint32_t arr = 1000000UL / freq;
    if (arr == 0) arr = 1;
    arr -= 1U;

    TIM3->CNT = 0;
    TIM3->ARR = arr;
    TIM3->CCR3 = arr / 2U;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->CCER |= TIM_CCER_CC3E;
    TIM3->CR1 |= TIM_CR1_CEN;
}

static void buzzer_off(void)
{
    TIM3->CCER &= ~TIM_CCER_CC3E;
    TIM3->CR1 &= ~TIM_CR1_CEN;
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = ms_ticks;
    while ((uint32_t)(ms_ticks - start) < ms)
    {
        if (reset_flag)
            return;
    }
}

static uint16_t adc_read(void)
{
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->SR & ADC_SR_EOC))
    {
        if (reset_flag) return 0;
    }
    return (uint16_t)(ADC1->DR & 0x0FFFU);
}

static Level read_level_from_pot(void)
{
    uint16_t v = adc_read();
    if (v < 1365U) return LV_SIMPLE;
    if (v < 2730U) return LV_MEDIUM;
    return LV_HARD;
}

static uint8_t button_down(uint8_t color)
{
    return ((BTN_PORT[color]->IDR & (1UL << BTN_BIT[color])) == 0U);
}

static uint8_t submit_down(void)
{
    return ((SUBMIT_PORT->IDR & (1UL << SUBMIT_BIT)) == 0U);
}

static uint8_t button_pressed(uint8_t color)
{
    if (!button_down(color)) return 0;
    delay_ms(20);
    return button_down(color);
}

static uint8_t submit_pressed(void)
{
    if (consume_submit_irq())
        return 1;

    if (!submit_down()) return 0;
    delay_ms(20);
    return submit_down();
}

static uint8_t consume_submit_irq(void)
{
    if (submit_irq_flag)
    {
        submit_irq_flag = 0;
        return 1;
    }
    return 0;
}

static void wait_release_color(uint8_t color)
{
    while (button_down(color))
    {
        if (reset_flag) return;
    }
    delay_ms(20);
}

static void wait_release_submit(void)
{
    while (submit_down())
    {
        if (reset_flag) return;
    }
    delay_ms(20);
}

/* ===================================================================== */
/* UART                                                                  */
/* ===================================================================== */
static void uart_putc(char c)
{
    while (!(USART2->SR & USART_SR_TXE)) { }
    USART2->DR = (uint8_t)c;
}

static void uprint(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n < 0) return;
    if (n >= (int)sizeof(buf)) n = sizeof(buf) - 1;

    for (int i = 0; i < n; i++)
        uart_putc(buf[i]);
}

/* ===================================================================== */
/* INTERRUPTS                                                            */
/* ===================================================================== */
void SysTick_Handler(void)
{
    ms_ticks++;
}

void EXTI15_10_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR13)
    {
        EXTI->PR = EXTI_PR_PR13;
        if ((uint32_t)(ms_ticks - submit_last_tick) >= 150U)
        {
            submit_last_tick = ms_ticks;
            submit_irq_flag = 1;
        }
    }

    if (EXTI->PR & EXTI_PR_PR14)
    {
        EXTI->PR = EXTI_PR_PR14;
        if ((uint32_t)(ms_ticks - reset_last_tick) >= 150U)
        {
            reset_last_tick = ms_ticks;
            reset_flag = 1;
        }
    }
}

/* ===================================================================== */
/* PERIPHERAL INIT                                                       */
/* ===================================================================== */
static void SystemClock_Config(void)
{
    /* 1. Turn ON HSI Oscillator (16 MHz) */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) { }

    /* 2. Configure Power Control & Flash Wait States for 84 MHz (2 WS) */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    FLASH->ACR = FLASH_ACR_LATENCY_2WS |
                 FLASH_ACR_PRFTEN |
                 FLASH_ACR_ICEN |
                 FLASH_ACR_DCEN;

    /* 3. Configure Prescalers: AHB = DIV1 (84MHz), APB1 = DIV2 (42MHz), APB2 = DIV1 (84MHz) */
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

    /* 4. Configure PLL: SYSCLK = (16MHz / 16) * 336 / 2 = 84 MHz */
    RCC->PLLCFGR = (16U << RCC_PLLCFGR_PLLM_Pos) |
                   (336U << RCC_PLLCFGR_PLLN_Pos) |
                   (0U << RCC_PLLCFGR_PLLP_Pos) |  /* 0U = DIV2 */
                   (7U << RCC_PLLCFGR_PLLQ_Pos);

    /* 5. Turn ON PLL and wait until Ready */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) { }

    /* 6. Select PLL as System Clock Source */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }
}

static void SysTick_Init(void)
{
    SysTick->LOAD = (SYSCLK_HZ / 1000UL) - 1UL;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}

static void GPIO_Init_All(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN |
                    RCC_AHB1ENR_GPIOBEN |
                    RCC_AHB1ENR_GPIOCEN;

    /* LEDs */
    GPIOA->MODER &= ~((3U << (5*2)) | (3U << (6*2)) | (3U << (7*2)));
    GPIOA->MODER |=  ((1U << (5*2)) | (1U << (6*2)) | (1U << (7*2)));
    GPIOB->MODER &= ~(3U << (6*2));
    GPIOB->MODER |=  (1U << (6*2));

    /* Color buttons: active LOW + internal pull-up */
    GPIOA->MODER &= ~(3U << (10*2));
    GPIOA->PUPDR &= ~(3U << (10*2));
    GPIOA->PUPDR |=  (1U << (10*2));

    GPIOB->MODER &= ~((3U << (3*2)) | (3U << (4*2)) | (3U << (5*2)));
    GPIOB->PUPDR &= ~((3U << (3*2)) | (3U << (4*2)) | (3U << (5*2)));
    GPIOB->PUPDR |=  ((1U << (3*2)) | (1U << (4*2)) | (1U << (5*2)));

    /* Submit PC13 + Reset PC14: input pull-up */
    GPIOC->MODER &= ~((3U << (13*2)) | (3U << (14*2)));
    GPIOC->PUPDR &= ~((3U << (13*2)) | (3U << (14*2)));
    GPIOC->PUPDR |=  ((1U << (13*2)) | (1U << (14*2)));

    /* Potentiometer PA4 = ADC1_IN4 */
    GPIOA->MODER |= (3U << (4*2));
    GPIOA->PUPDR &= ~(3U << (4*2));

    /* Buzzer PB0 = TIM3_CH3, AF2 */
    GPIOB->MODER &= ~(3U << (0*2));
    GPIOB->MODER |=  (2U << (0*2));
    GPIOB->AFR[0] &= ~(0xFU << (0*4));
    GPIOB->AFR[0] |=  (2U << (0*4));

    /* USART2 PA2 TX / PA3 RX = AF7 */
    GPIOA->MODER &= ~((3U << (2*2)) | (3U << (3*2)));
    GPIOA->MODER |=  ((2U << (2*2)) | (2U << (3*2)));
    GPIOA->AFR[0] &= ~((0xFU << (2*4)) | (0xFU << (3*4)));
    GPIOA->AFR[0] |=  ((7U << (2*4)) | (7U << (3*4)));
}

static void ADC1_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    ADC1->CR1 = 0;
    ADC1->CR2 = 0;
    ADC1->SMPR2 &= ~(7U << (4*3));
    ADC1->SMPR2 |=  (7U << (4*3));
    ADC1->SQR1 &= ~ADC_SQR1_L;
    ADC1->SQR3 = 4U;
    ADC1->CR2 |= ADC_CR2_ADON;
}

static void TIM3_PWM_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /* TIM3 clock = 84 MHz (APB1 Timer Clock = APB1 Clock * 2) */
    TIM3->PSC = 84U - 1U;      /* 1 MHz timer counter */
    TIM3->ARR = 1000U - 1U;
    TIM3->CCR3 = 500U;

    TIM3->CCMR2 &= ~TIM_CCMR2_OC3M;
    TIM3->CCMR2 |= (6U << TIM_CCMR2_OC3M_Pos);
    TIM3->CCMR2 |= TIM_CCMR2_OC3PE;

    TIM3->CCER &= ~TIM_CCER_CC3E;
    TIM3->CR1 &= ~TIM_CR1_CEN;
}

static void USART2_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* คำนวณค่า BRR สำหรับ 115200 Baud Rate อัตโนมัติจาก APB1 Clock (42 MHz) */
    uint32_t baudrate = 115200UL;
    USART2->BRR = (APB1CLK_HZ + (baudrate / 2U)) / baudrate;

    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    USART2->CR2 = 0;
    USART2->CR3 = 0;

    while (!(USART2->SR & USART_SR_TXE)) { }
}

static void EXTI14_Reset_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* EXTI13 -> Port C */
    SYSCFG->EXTICR[3] &= ~(0xFU << 4);
    SYSCFG->EXTICR[3] |=  (0x2U << 4);

    /* EXTI14 -> Port C */
    SYSCFG->EXTICR[3] &= ~(0xFU << 8);
    SYSCFG->EXTICR[3] |=  (0x2U << 8);

    EXTI->IMR |= EXTI_IMR_MR13 | EXTI_IMR_MR14;
    EXTI->FTSR |= EXTI_FTSR_TR13 | EXTI_FTSR_TR14;
    EXTI->RTSR &= ~(EXTI_RTSR_TR13 | EXTI_RTSR_TR14);

    EXTI->PR = EXTI_PR_PR13 | EXTI_PR_PR14;

    NVIC_SetPriority(EXTI15_10_IRQn, 1);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}