#include "ch32v00x_conf.h"
#include "user_flash.h"

/* 16К flash, страницы по 64 байта. Резервируем две последние. */
#define FL_PAGE_SIZE      64U
#define FL_PAGE0_ADDR     0x08003F80UL   /* страница 254 */
#define FL_PAGE1_ADDR     0x08003FC0UL   /* страница 255 */
#define FL_REC_SIZE       4U
#define FL_RECS_PER_PAGE  (FL_PAGE_SIZE / FL_REC_SIZE)
#define FL_MARKER         0xAU           /* старший ниббл записи */
#define FL_EMPTY          0xFFFFU

static const uint32_t pageAddr[2] = { FL_PAGE0_ADDR, FL_PAGE1_ADDR };

static uint8_t  activePage = 0;    /* страница, куда пишем следующую запись */
static uint8_t  recIndex   = 0;    /* индекс следующего свободного слота */
static uint16_t lastHW0    = 0;    /* упакованное значение новейшей записи */
static uint8_t  hasData    = 0;

/* Упаковка: [15:12] маркер | [11:4] val (int8) | [3:2] резерв=0 | [1:0] state */
static uint16_t PackRec(uint8_t state, int8_t val) {
    if (state > FL_ST_SLEEP) state = FL_ST_IDLE;   /* страховка */
    return (uint16_t)((FL_MARKER << 12)
                    | ((uint16_t)(uint8_t)val << 4)
                    | (uint16_t)(state & 0x03));
}

static uint16_t ReadHW(uint8_t page, uint8_t slot, uint8_t half) {
    return *(volatile uint16_t *)(pageAddr[page]
                + (uint32_t)slot * FL_REC_SIZE + (uint32_t)half * 2U);
}

static uint8_t RecValid(uint16_t hw0, uint16_t hw1) {
    if (hw1 != (uint16_t)(~hw0))       return 0;
    if ((hw0 >> 12) != FL_MARKER)      return 0;
    if ((hw0 >> 2) & 0x3)              return 0;   /* резервные биты должны быть 0 */
    return 1;
}

uint8_t Flash_Init(void) {
    uint8_t  v[2] = {0, 0};    /* валидных записей */
    uint8_t  f[2] = {0, 0};    /* свободных слотов */
    uint8_t  g[2] = {0, 0};    /* мусорных слотов */
    uint8_t  ff[2] = {FL_RECS_PER_PAGE, FL_RECS_PER_PAGE}; /* первый свободный */
    uint16_t lv[2] = {0, 0};   /* последнее валидное значение */

    for (uint8_t p = 0; p < 2; p++) {
        for (uint8_t s = 0; s < FL_RECS_PER_PAGE; s++) {
            uint16_t hw0 = ReadHW(p, s, 0);
            uint16_t hw1 = ReadHW(p, s, 1);
            if (hw0 == FL_EMPTY && hw1 == FL_EMPTY) {
                if (f[p] == 0U) ff[p] = s;
                f[p]++;
            } else if (RecValid(hw0, hw1)) {
                v[p]++; lv[p] = hw0;
            } else {
                g[p]++;   /* недописанная/битая запись — слот мёртв до ближайшей стирки */
            }
        }
    }

    hasData = 0U;

    /* Нет валидных записей: чистая память или мусор */
    if (v[0] == 0U && v[1] == 0U) {
        if (g[0]) FLASH_ErasePage(pageAddr[0]);
        if (g[1]) FLASH_ErasePage(pageAddr[1]);
        activePage = 0; recIndex = 0; lastHW0 = 0;
        return 0;
    }

    if (v[0] && v[1]) {
        /* Штатно: одна страница полная, вторая частичная */
        if (f[0] && f[1]) {   /* аномалия: обе частичные — чистим всё */
            FLASH_ErasePage(pageAddr[0]);
            FLASH_ErasePage(pageAddr[1]);
            activePage = 0; recIndex = 0; lastHW0 = 0;
            return 0;
        }
        activePage = (f[0] != 0U) ? 0 : 1;
        recIndex   = ff[activePage];
        lastHW0    = lv[activePage];
        hasData    = 1U;
        return 1;
    }

    /* Валидные записи только на одной странице */
    uint8_t p = (v[0] != 0U) ? 0 : 1;
    if (f[p] != 0U) {
        activePage = p; recIndex = ff[p]; lastHW0 = lv[p]; hasData = 1U;
        return 1;
    }
    /* Страница заполнена целиком — переключаемся на вторую.
       Стираем её, только если там что-то лежит (чтобы не жечь флеш зря при каждом ребуте). */
    activePage = (uint8_t)(1U - p);
    if (f[activePage] != FL_RECS_PER_PAGE || g[activePage] != 0U) {
        FLASH_ErasePage(pageAddr[activePage]);
    }
    recIndex = 0; lastHW0 = lv[p]; hasData = 1U;
    return 1;
}

uint8_t Flash_Load(uint8_t *state, int8_t *val) {
    if (!hasData) return 0;
    *state = (uint8_t)(lastHW0 & 0x03);
    *val   = (int8_t)((lastHW0 >> 4) & 0xFF);
    return 1;
}

void Flash_Save(uint8_t state, int8_t val) {
    uint16_t hw0 = PackRec(state, val);

    if (hw0 == lastHW0) return;   /* ничего не изменилось — не пишем */

    if (recIndex >= FL_RECS_PER_PAGE) {
        /* активная страница заполнена — стираем другую и переключаемся */
        uint8_t other = (uint8_t)((activePage == 0U) ? 1U : 0U);
        FLASH_ErasePage(pageAddr[other]);
        activePage = other;
        recIndex = 0;
    }

    uint16_t hw1  = (uint16_t)(~hw0);
    uint32_t addr = pageAddr[activePage] + (uint32_t)recIndex * FL_REC_SIZE;

    /* программирование стопорит шину — исключаем тики и АЦП на время записи */
    NVIC_DisableIRQ(SysTick_IRQn);
    NVIC_DisableIRQ(ADC_IRQn);

    FLASH_Unlock();
    FLASH_ProgramHalfWord(addr,     hw0);
    FLASH_ProgramHalfWord(addr + 2, hw1);
    FLASH_Lock();

    NVIC_EnableIRQ(SysTick_IRQn);
    NVIC_EnableIRQ(ADC_IRQn);

    lastHW0 = hw0;
    hasData = 1U;
    recIndex++;
}
