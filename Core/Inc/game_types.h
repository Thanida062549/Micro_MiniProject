#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <stdint.h>

/* ----------------- ค่าคงที่ของเกม ----------------- */
#define TOTAL_ROUNDS              5    /* จำนวนด่านทั้งหมดต่อเกม */
#define HP_INIT                   3    /* พลังชีวิตเริ่มต้น (ด่านที่มีการกดผิด 3 ด่าน = game over) */
#define PATTERN_MAX_LEN           8    /* ความยาว pattern สูงสุด (โหมดยาก = 8 ดวงทุกด่าน) */

#define NO_PRESS_PROBABILITY_PCT  35   /* โอกาส % ที่จะมีสีต้องห้ามในด่านนี้ */
#define BUZZ_PROBABILITY_PCT      40   /* โอกาส % ที่ต้องตอบย้อนลำดับ (buzzer ดัง) */

/* ----------------- ระดับความยาก (เลือกจาก potentiometer + ปุ่ม 1 ยืนยัน ตอนเริ่มเกม) -----------------
 * ความยาก "คงที่ตลอดทั้ง 5 ด่าน" ไม่ไต่ระดับต่อด่านแล้ว:
 *   EASY   = 4 ดวงทุกด่าน
 *   MEDIUM = 6 ดวงทุกด่าน
 *   HARD   = 8 ดวงทุกด่าน */
typedef enum {
    DIFFICULTY_EASY   = 0,
    DIFFICULTY_MEDIUM = 1,
    DIFFICULTY_HARD   = 2
} Difficulty_t;

/* ----------------- โครงสร้างข้อมูล pattern ----------------- */
typedef struct {
    uint8_t data[PATTERN_MAX_LEN];   /* เก็บลำดับสี/ปุ่ม ค่า 1-4 ต่อช่อง (1=แดง 2=เหลือง 3=เขียว 4=น้ำเงิน) */
    uint8_t length;                  /* ความยาวที่ใช้จริง */
} Pattern_t;

/* ----------------- state หลักของเกม ----------------- */
typedef enum {
    STATE_START = 0,
    STATE_MEMORY_GAME,
    STATE_DISPLAY_SELECT_LEVEL,
    STATE_RANDOM_NO_PRESS,
    STATE_SHOW_PATTERN,
    STATE_RANDOM_BUZZ,
    STATE_ANS_PRESS_COLLECT,
    STATE_CHECK_ANSWER,
    STATE_DISPLAY_CORRECT,
    STATE_DISPLAY_INCORRECT_HP,
    STATE_DISPLAY_ACCURACY,
    STATE_GAME_OVER
} GameState_t;

/* ----------------- context ที่เก็บค่าตลอดเกม ----------------- */
typedef struct {
    GameState_t state;
    uint8_t round;              /* ด่านปัจจุบัน 1..TOTAL_ROUNDS */
    uint8_t hp;                  /* พลังชีวิตที่เหลือ */
    uint8_t accuracy_count;      /* จำนวนด่านที่ตอบถูกทั้งหมด (ไม่มีกดผิดเลยสักครั้ง) สะสมทั้งเกม */
    uint8_t no_press_color;      /* 0 = ไม่มีข้อจำกัด, 1-4 = สีที่ห้ามกดในด่านนี้ (สุ่มใหม่ทุกด่าน) */
    uint8_t is_reverse_round;    /* 1 ถ้าด่านนี้ buzzer ดัง ต้องตอบย้อนลำดับ (สุ่มใหม่ทุกด่าน) */
    Difficulty_t difficulty;     /* เลือกครั้งเดียวตอนเริ่มเกม คงที่ตลอดทั้ง 5 ด่าน */
    Pattern_t question;          /* Store_Question_Pattern */
    Pattern_t answer;            /* Store_Answer_Pattern */
} GameContext_t;

/* ----------------- ผลลัพธ์การเล่น 1 ด่าน ----------------- */
typedef enum {
    ROUND_CONTINUE = 0,
    ROUND_ACCURACY_DONE,
    ROUND_GAME_OVER
} RoundOutcome_t;

#endif /* GAME_TYPES_H */
