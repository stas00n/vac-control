#include "ch32v00x_conf.h"
#include "user_flash.h"

/* 16§¬ flash, §ã§ä§â§Ñ§ß§Ú§è§í §á§à 64 §Ò§Ñ§Û§ä§Ñ. §²§Ö§Ù§Ö§â§Ó§Ú§â§å§Ö§Þ §Õ§Ó§Ö §á§à§ã§Ý§Ö§Õ§ß§Ú§Ö. */
#define FL_PAGE_SIZE      64U
#define FL_PAGE0_ADDR     0x08003F80UL   /* §ã§ä§â§Ñ§ß§Ú§è§Ñ 254 */
#define FL_PAGE1_ADDR     0x08003FC0UL   /* §ã§ä§â§Ñ§ß§Ú§è§Ñ 255 */
#define FL_REC_SIZE       4U
#define FL_RECS_PER_PAGE  (FL_PAGE_SIZE / FL_REC_SIZE)
#define FL_MARKER         0xAU           /* §ã§ä§Ñ§â§ê§Ú§Û §ß§Ú§Ò§Ò§Ý §Ù§Ñ§á§Ú§ã§Ú */
#define FL_EMPTY          0xFFFFU

static const uint32_t pageAddr[2] = { FL_PAGE0_ADDR, FL_PAGE1_ADDR };

static uint8_t  activePage = 0;    /* §ã§ä§â§Ñ§ß§Ú§è§Ñ, §Ü§å§Õ§Ñ §á§Ú§ê§Ö§Þ §ã§Ý§Ö§Õ§å§ð§ë§å§ð §Ù§Ñ§á§Ú§ã§î */
static uint8_t  recIndex   = 0;    /* §Ú§ß§Õ§Ö§Ü§ã §ã§Ý§Ö§Õ§å§ð§ë§Ö§Ô§à §ã§Ó§à§Ò§à§Õ§ß§à§Ô§à §ã§Ý§à§ä§Ñ */
static uint16_t lastHW0    = 0;    /* §å§á§Ñ§Ü§à§Ó§Ñ§ß§ß§à§Ö §Ù§ß§Ñ§é§Ö§ß§Ú§Ö §ß§à§Ó§Ö§Û§ê§Ö§Û §Ù§Ñ§á§Ú§ã§Ú */
static uint8_t  hasData    = 0;

/* §µ§á§Ñ§Ü§à§Ó§Ü§Ñ: [15:12] §Þ§Ñ§â§Ü§Ö§â | [11:4] val (int8) | [3:2] §â§Ö§Ù§Ö§â§Ó=0 | [1:0] state */
static uint16_t PackRec(uint8_t state, int8_t val) {
    if (state > FL_ST_SLEEP) state = FL_ST_IDLE;   /* §ã§ä§â§Ñ§ç§à§Ó§Ü§Ñ */
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
    if ((hw0 >> 2) & 0x3)              return 0;   /* §â§Ö§Ù§Ö§â§Ó§ß§í§Ö §Ò§Ú§ä§í §Õ§à§Ý§Ø§ß§í §Ò§í§ä§î 0 */
    return 1;
}

uint8_t Flash_Init(void) {
    uint8_t  v[2] = {0, 0};    /* §Ó§Ñ§Ý§Ú§Õ§ß§í§ç §Ù§Ñ§á§Ú§ã§Ö§Û */
    uint8_t  f[2] = {0, 0};    /* §ã§Ó§à§Ò§à§Õ§ß§í§ç §ã§Ý§à§ä§à§Ó */
    uint8_t  g[2] = {0, 0};    /* §Þ§å§ã§à§â§ß§í§ç §ã§Ý§à§ä§à§Ó */
    uint8_t  ff[2] = {FL_RECS_PER_PAGE, FL_RECS_PER_PAGE}; /* §á§Ö§â§Ó§í§Û §ã§Ó§à§Ò§à§Õ§ß§í§Û */
    uint16_t lv[2] = {0, 0};   /* §á§à§ã§Ý§Ö§Õ§ß§Ö§Ö §Ó§Ñ§Ý§Ú§Õ§ß§à§Ö §Ù§ß§Ñ§é§Ö§ß§Ú§Ö */

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
                g[p]++;   /* §ß§Ö§Õ§à§á§Ú§ã§Ñ§ß§ß§Ñ§ñ/§Ò§Ú§ä§Ñ§ñ §Ù§Ñ§á§Ú§ã§î ¡ª §ã§Ý§à§ä §Þ§×§â§ä§Ó §Õ§à §Ò§Ý§Ú§Ø§Ñ§Û§ê§Ö§Û §ã§ä§Ú§â§Ü§Ú */
            }
        }
    }

    hasData = 0U;

    /* §¯§Ö§ä §Ó§Ñ§Ý§Ú§Õ§ß§í§ç §Ù§Ñ§á§Ú§ã§Ö§Û: §é§Ú§ã§ä§Ñ§ñ §á§Ñ§Þ§ñ§ä§î §Ú§Ý§Ú §Þ§å§ã§à§â */
    if (v[0] == 0U && v[1] == 0U) {
        if (g[0]) FLASH_ErasePage(pageAddr[0]);
        if (g[1]) FLASH_ErasePage(pageAddr[1]);
        activePage = 0; recIndex = 0; lastHW0 = 0;
        return 0;
    }

    if (v[0] && v[1]) {
        /* §º§ä§Ñ§ä§ß§à: §à§Õ§ß§Ñ §ã§ä§â§Ñ§ß§Ú§è§Ñ §á§à§Ý§ß§Ñ§ñ, §Ó§ä§à§â§Ñ§ñ §é§Ñ§ã§ä§Ú§é§ß§Ñ§ñ */
        if (f[0] && f[1]) {   /* §Ñ§ß§à§Þ§Ñ§Ý§Ú§ñ: §à§Ò§Ö §é§Ñ§ã§ä§Ú§é§ß§í§Ö ¡ª §é§Ú§ã§ä§Ú§Þ §Ó§ã§× */
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

    /* §£§Ñ§Ý§Ú§Õ§ß§í§Ö §Ù§Ñ§á§Ú§ã§Ú §ä§à§Ý§î§Ü§à §ß§Ñ §à§Õ§ß§à§Û §ã§ä§â§Ñ§ß§Ú§è§Ö */
    uint8_t p = (v[0] != 0U) ? 0 : 1;
    if (f[p] != 0U) {
        activePage = p; recIndex = ff[p]; lastHW0 = lv[p]; hasData = 1U;
        return 1;
    }
    /* §³§ä§â§Ñ§ß§Ú§è§Ñ §Ù§Ñ§á§à§Ý§ß§Ö§ß§Ñ §è§Ö§Ý§Ú§Ü§à§Þ ¡ª §á§Ö§â§Ö§Ü§Ý§ð§é§Ñ§Ö§Þ§ã§ñ §ß§Ñ §Ó§ä§à§â§å§ð.
       §³§ä§Ú§â§Ñ§Ö§Þ §Ö§×, §ä§à§Ý§î§Ü§à §Ö§ã§Ý§Ú §ä§Ñ§Þ §é§ä§à-§ä§à §Ý§Ö§Ø§Ú§ä (§é§ä§à§Ò§í §ß§Ö §Ø§Ö§é§î §æ§Ý§Ö§ê §Ù§â§ñ §á§â§Ú §Ü§Ñ§Ø§Õ§à§Þ §â§Ö§Ò§å§ä§Ö). */
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

    if (hw0 == lastHW0) return;   /* §ß§Ú§é§Ö§Ô§à §ß§Ö §Ú§Ù§Þ§Ö§ß§Ú§Ý§à§ã§î ¡ª §ß§Ö §á§Ú§ê§Ö§Þ */

    if (recIndex >= FL_RECS_PER_PAGE) {
        /* §Ñ§Ü§ä§Ú§Ó§ß§Ñ§ñ §ã§ä§â§Ñ§ß§Ú§è§Ñ §Ù§Ñ§á§à§Ý§ß§Ö§ß§Ñ ¡ª §ã§ä§Ú§â§Ñ§Ö§Þ §Õ§â§å§Ô§å§ð §Ú §á§Ö§â§Ö§Ü§Ý§ð§é§Ñ§Ö§Þ§ã§ñ */
        uint8_t other = (uint8_t)((activePage == 0U) ? 1U : 0U);
        FLASH_ErasePage(pageAddr[other]);
        activePage = other;
        recIndex = 0;
    }

    uint16_t hw1  = (uint16_t)(~hw0);
    uint32_t addr = pageAddr[activePage] + (uint32_t)recIndex * FL_REC_SIZE;

    /* §á§â§à§Ô§â§Ñ§Þ§Þ§Ú§â§à§Ó§Ñ§ß§Ú§Ö §ã§ä§à§á§à§â§Ú§ä §ê§Ú§ß§å ¡ª §Ú§ã§Ü§Ý§ð§é§Ñ§Ö§Þ §ä§Ú§Ü§Ú §Ú §¡§¸§± §ß§Ñ §Ó§â§Ö§Þ§ñ §Ù§Ñ§á§Ú§ã§Ú */
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
