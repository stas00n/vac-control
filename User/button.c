#include "io_conf.h"
#include "ch32v00x_conf.h"
#include "button.h"

#define DEBOUNCE_TICK_COUNT      10U       /* §Þ§ã §á§à§Õ§Ñ§Ó§Ý§Ö§ß§Ú§ñ §Õ§â§Ö§Ò§Ö§Ù§Ô§Ñ */
#define DOUBLE_CLICK_TIMEOUT_MS  300U      /* §à§Ü§ß§à §Þ§Ö§Ø§Õ§å §Ü§Ý§Ú§Ü§Ñ§Þ§Ú */
#define HOLD_TIME_MS             1000U     /* §á§à§â§à§Ô §Õ§Ý§Ú§ä§Ö§Ý§î§ß§à§Ô§à §ß§Ñ§Ø§Ñ§ä§Ú§ñ */

/* §¤§â§å§Ò§Ñ§ñ §Ù§Ñ§Õ§Ö§â§Ø§Ü§Ñ ~1 §Þ§ã, §ß§Ö §Ù§Ñ§Ó§Ú§ã§Ú§ä §à§ä SysTick (§ß§å§Ø§ß§Ñ §Õ§Ý§ñ Button_Enable).
   §¬§à§ï§æ§æ§Ú§è§Ú§Ö§ß§ä 5 ¡ª §á§â§Ú§Þ§Ö§â§ß§à §ä§Ñ§Ü§ä§à§Ó §ß§Ñ §Ú§ä§Ö§â§Ñ§è§Ú§ð volatile-§è§Ú§Ü§Ý§Ñ. */
extern uint32_t SystemCoreClock;
static void BusyDelayMs(uint32_t ms) {
    while (ms--) {
        for (volatile uint32_t i = 0; i < (SystemCoreClock / 5000U); i++);
    }
}

/* -----------------------------------------------------------------------------
 * §®§Ñ§ê§Ú§ß§Ñ §ã§à§ã§ä§à§ñ§ß§Ú§Û
 * ---------------------------------------------------------------------------*/
typedef enum {
    SM_WAIT_PRESS,        /* §Ø§Õ§×§Þ §ß§Ñ§Ø§Ñ§ä§Ú§ñ */
    SM_DEBOUNCE_PRESS,    /* §Õ§â§Ö§Ò§Ö§Ù§Ô §á§â§Ú §ß§Ñ§Ø§Ñ§ä§Ú§Ú */
    SM_WAIT_RELEASE,      /* §Ø§Õ§×§Þ §à§ä§á§å§ã§Ü§Ñ§ß§Ú§ñ §Ú§Ý§Ú §Ú§ã§ä§Ö§é§Ö§ß§Ú§ñ HOLD_TIME */
    SM_HOLD_DETECTED,     /* HOLD §Ù§Ñ§æ§Ú§Ü§ã§Ú§â§à§Ó§Ñ§ß, §Ø§Õ§×§Þ §à§ä§á§å§ã§Ü§Ñ§ß§Ú§ñ */
    SM_DEBOUNCE_RELEASE,  /* §Õ§â§Ö§Ò§Ö§Ù§Ô §á§â§Ú §à§ä§á§å§ã§Ü§Ñ§ß§Ú§Ú */
    SM_WAIT_SECOND_PRESS  /* §à§Ü§ß§à §à§Ø§Ú§Õ§Ñ§ß§Ú§ñ §Ó§ä§à§â§à§Ô§à §Ü§Ý§Ú§Ü§Ñ */
} ButtonState;

static volatile uint8_t btnEnabled = 0;
static TBTNEVT     btnEvt = BTN_EVT_NONE;
static ButtonState btnState = SM_WAIT_PRESS;
static uint32_t    debounceCounter  = 0;
static uint32_t    doubleClickTimer = 0;
static uint32_t    holdTimer        = 0;
static uint8_t     holdOccurred     = 0;  /* §á§à§ã§Ý§Ö HOLD CLICK §ß§Ö §Ô§Ö§ß§Ö§â§Ú§â§å§Ö§Þ */
static uint8_t     dblCandidate     = 0;  /* §ï§ä§à §ß§Ñ§Ø§Ñ§ä§Ú§Ö ¡ª §Ü§Ñ§ß§Õ§Ú§Õ§Ñ§ä §Ó DOUBLE */

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
                /* §å§Õ§Ö§â§Ø§Ñ§ß§Ú§Ö §á§â§Ö§Ó§í§ã§Ú§Ý§à HOLD_TIME ¡ª §ï§ä§à HOLD */
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
                /* §Ü§à§â§à§ä§Ü§à§Ö §á§à§Ó§ä§à§â§ß§à§Ö §ß§Ñ§Ø§Ñ§ä§Ú§Ö ¡ª §Ü§Ñ§ß§Õ§Ú§Õ§Ñ§ä §Ó §Õ§Ó§à§Û§ß§à§Û §Ü§Ý§Ú§Ü */
                dblCandidate = (doubleClickTimer > 0U) ? 1U : 0U;
                btnState = SM_DEBOUNCE_PRESS;
                debounceCounter = DEBOUNCE_TICK_COUNT;
            } else if (debounceCounter > 0U) {
                debounceCounter--;
            } else if (holdOccurred) {
                /* §á§à§ã§Ý§Ö HOLD §Ý§Ú§ê§î §Õ§à§Ø§Õ§Ñ§ä§î§ã§ñ §à§ä§á§å§ã§Ü§Ñ§ß§Ú§ñ §Ú §Ø§Õ§Ñ§ä§î §ß§à§Ó§à§Ô§à §ß§Ñ§Ø§Ñ§ä§Ú§ñ */
                holdOccurred = 0U;
                btnState = SM_WAIT_PRESS;
            } else if (dblCandidate) {
                /* §Ó§ä§à§â§à§Û §Ü§Ý§Ú§Ü §á§â§Ú§ê§×§Ý §Ó §à§Ü§ß§à ¡ª DOUBLE */
                btnEvt = BTN_EVT_DOUBLE;
                doubleClickTimer = 0U;
                dblCandidate = 0U;
                btnState = SM_WAIT_PRESS;
            } else {
                /* §å§ã§ä§à§Û§é§Ú§Ó§à§Ö §à§ä§á§å§ã§Ü§Ñ§ß§Ú§Ö ¡ª CLICK, §à§Ü§ß§à §ß§Ñ §Õ§Ó§à§Û§ß§à§Û §Ü§Ý§Ú§Ü §à§ä§Ü§â§í§ä§à */
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
                    /* §à§Ü§ß§à §Ú§ã§ä§Ö§Ü§Ý§à ¡ª §ï§ä§à §å§Ø§Ö §à§Ò§í§é§ß§à§Ö §ß§à§Ó§à§Ö §ß§Ñ§Ø§Ñ§ä§Ú§Ö */
                    btnState = SM_WAIT_PRESS;
                }
            } else if (doubleClickTimer > 0U) {
                doubleClickTimer--;
            } else {
                /* CLICK §ß§Ö §ã§ä§Ú§â§Ñ§Ö§Þ ¡ª §Ö§Ô§à §Õ§à§Ý§Ø§Ö§ß §Ù§Ñ§Ò§â§Ñ§ä§î §à§ã§ß§à§Ó§ß§à§Û §è§Ú§Ü§Ý */
                btnState = SM_WAIT_PRESS;
            }
            break;
    }
}

void Button_Enable(void) {
    btnEnabled = 0U;

    /* §ã§Ò§â§à§ã §Þ§Ñ§ê§Ú§ß§í §Ú §ã§à§Ò§í§ä§Ú§Û */
    btnEvt = BTN_EVT_NONE;
    btnState = SM_WAIT_PRESS;
    debounceCounter = 0U;
    doubleClickTimer = 0U;
    holdTimer = 0U;
    holdOccurred = 0U;
    dblCandidate = 0U;

    /* §Ø§Õ§×§Þ, §á§à§Ü§Ñ §Ü§ß§à§á§Ü§Ñ §å§ã§ä§à§Û§é§Ú§Ó§à §à§ä§á§å§ë§Ö§ß§Ñ: DEBOUNCE_TICK_COUNT §Þ§ã §á§à§Õ§â§ñ§Õ.
       §°§á§â§à§ã §ß§Ñ§á§â§ñ§Þ§å§ð §é§Ö§â§Ö§Ù GPIO, BusyDelay §ß§Ö §Ù§Ñ§Ó§Ú§ã§Ú§ä §à§ä SysTick. */
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
