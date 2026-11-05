#include "io_conf.h"
#include "ch32v00x_conf.h"
#include "button.h"

#define DEBOUNCE_TICK_COUNT      10U       /* мс подавления дребезга */
#define DOUBLE_CLICK_TIMEOUT_MS  300U      /* окно между кликами */
#define HOLD_TIME_MS             1000U     /* порог длительного нажатия */

/* Грубая задержка ~1 мс, не зависит от SysTick (нужна для Button_Enable).
   Коэффициент 5 — примерно тактов на итерацию volatile-цикла. */
extern uint32_t SystemCoreClock;
static void BusyDelayMs(uint32_t ms) {
    while (ms--) {
        for (volatile uint32_t i = 0; i < (SystemCoreClock / 5000U); i++);
    }
}

/* -----------------------------------------------------------------------------
 * Машина состояний
 * ---------------------------------------------------------------------------*/
typedef enum {
    SM_WAIT_PRESS,        /* ждём нажатия */
    SM_DEBOUNCE_PRESS,    /* дребезг при нажатии */
    SM_WAIT_RELEASE,      /* ждём отпускания или истечения HOLD_TIME */
    SM_HOLD_DETECTED,     /* HOLD зафиксирован, ждём отпускания */
    SM_DEBOUNCE_RELEASE,  /* дребезг при отпускании */
    SM_WAIT_SECOND_PRESS  /* окно ожидания второго клика */
} ButtonState;

static volatile uint8_t btnEnabled = 0;
static TBTNEVT     btnEvt = BTN_EVT_NONE;
static ButtonState btnState = SM_WAIT_PRESS;
static uint32_t    debounceCounter  = 0;
static uint32_t    doubleClickTimer = 0;
static uint32_t    holdTimer        = 0;
static uint8_t     holdOccurred     = 0;  /* после HOLD CLICK не генерируем */
static uint8_t     dblCandidate     = 0;  /* это нажатие — кандидат в DOUBLE */

static bool Button_IsPressed(void) {
    return !GPIO_ReadInputDataBit(PIN_BTN);
}

void Button_OnTick(void) {
    if (!btnEnabled) return;

    bool pressed = Button_IsPressed();

    switch (btnState) {
        case SM_WAIT_PRESS:
            if (pressed) {
                dblCandidate = (doubleClickTimer > 0U) ? 1U : 0U;
                btnState = SM_DEBOUNCE_PRESS;
                debounceCounter = DEBOUNCE_TICK_COUNT;
            }
            break;

        case SM_DEBOUNCE_PRESS:
            if (!pressed) {
                btnState = SM_WAIT_PRESS;
            } else if (--debounceCounter == 0U) {
                holdTimer = HOLD_TIME_MS;
                btnState = SM_WAIT_RELEASE;
            }
            break;

        case SM_WAIT_RELEASE:
            if (!pressed) {
                btnState = SM_DEBOUNCE_RELEASE;
                debounceCounter = DEBOUNCE_TICK_COUNT;
            } else if (holdTimer > 0U) {
                holdTimer--;
            } else {
                /* удержание превысило HOLD_TIME — это HOLD */
                btnEvt = BTN_EVT_HOLD;
                holdOccurred = 1U;
                dblCandidate = 0U;
                btnState = SM_HOLD_DETECTED;
            }
            break;

        case SM_HOLD_DETECTED:
            if (!pressed) {
                btnState = SM_DEBOUNCE_RELEASE;
                debounceCounter = DEBOUNCE_TICK_COUNT;
            }
            break;

        case SM_DEBOUNCE_RELEASE:
            if (pressed) {
                /* короткое повторное нажатие — кандидат в двойной клик */
                dblCandidate = (doubleClickTimer > 0U) ? 1U : 0U;
                btnState = SM_DEBOUNCE_PRESS;
                debounceCounter = DEBOUNCE_TICK_COUNT;
            } else if (debounceCounter > 0U) {
                debounceCounter--;
            } else if (holdOccurred) {
                /* после HOLD лишь дождаться отпускания и ждать нового нажатия */
                holdOccurred = 0U;
                btnState = SM_WAIT_PRESS;
            } else if (dblCandidate) {
                /* второй клик пришёл в окно — DOUBLE */
                btnEvt = BTN_EVT_DOUBLE;
                doubleClickTimer = 0U;
                dblCandidate = 0U;
                btnState = SM_WAIT_PRESS;
            } else {
                /* устойчивое отпускание — CLICK, окно на двойной клик открыто */
                btnEvt = BTN_EVT_CLICK;
                doubleClickTimer = DOUBLE_CLICK_TIMEOUT_MS;
                btnState = SM_WAIT_SECOND_PRESS;
            }
            break;

        case SM_WAIT_SECOND_PRESS:
            if (pressed) {
                if (doubleClickTimer > 0U) {
                    dblCandidate = 1U;
                    btnState = SM_DEBOUNCE_PRESS;
                    debounceCounter = DEBOUNCE_TICK_COUNT;
                } else {
                    /* окно истекло — это уже обычное новое нажатие */
                    btnState = SM_WAIT_PRESS;
                }
            } else if (doubleClickTimer > 0U) {
                doubleClickTimer--;
            } else {
                /* CLICK не стираем — его должен забрать основной цикл */
                btnState = SM_WAIT_PRESS;
            }
            break;
    }
}

void Button_Enable(void) {
    btnEnabled = 0U;

    /* сброс машины и событий */
    btnEvt = BTN_EVT_NONE;
    btnState = SM_WAIT_PRESS;
    debounceCounter = 0U;
    doubleClickTimer = 0U;
    holdTimer = 0U;
    holdOccurred = 0U;
    dblCandidate = 0U;

    /* ждём, пока кнопка устойчиво отпущена: DEBOUNCE_TICK_COUNT мс подряд.
       Опрос напрямую через GPIO, BusyDelay не зависит от SysTick. */
    uint32_t stable = 0U;
    while (stable < DEBOUNCE_TICK_COUNT) {
        if (Button_IsPressed()) {
            stable = 0U;
        } else {
            stable++;
        }
        BusyDelayMs(1U);
    }

    btnEnabled = 1U;
}

void Button_Disable(void) {
    btnEnabled = 0U;
}

TBTNEVT Button_GetEvt(void) {
    TBTNEVT evt = btnEvt;
    if (evt != BTN_EVT_NONE) {
        btnEvt = BTN_EVT_NONE;
    }
    return evt;
}
