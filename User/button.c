#include "io_conf.h"
#include "ch32v00x_conf.h"
#include "button.h"
#include <stdbool.h>

/* -----------------------------------------------------------------------------
 * §¬§à§ß§æ§Ú§Ô§å§â§Ñ§è§Ú§ñ §ä§Ñ§Û§Þ§Ú§ß§Ô§à§Ó (§á§à§Õ§Ò§Ú§â§Ñ§Ö§ä§ã§ñ §á§à§Õ §á§Ý§Ñ§ä§å §Ú §Ü§ß§à§á§Ü§å)
 * ---------------------------------------------------------------------------*/
#define DEBOUNCE_TICK_COUNT      10U       /* §ä§Ú§Ü§à§Ó §á§à 1 §Þ§ã §Õ§Ý§ñ §á§à§Õ§Ñ§Ó§Ý§Ö§ß§Ú§ñ §Õ§â§Ö§Ò§Ö§Ù§Ô§Ñ */
#define DOUBLE_CLICK_TIMEOUT_MS  300U      /* §Þ§Ñ§Ü§ã. §Ù§Ñ§Õ§Ö§â§Ø§Ü§Ñ §Þ§Ö§Ø§Õ§å 1-§Þ §Ú 2-§Þ §ß§Ñ§Ø§Ñ§ä§Ú§Ö§Þ */

/* -----------------------------------------------------------------------------
 * §­§à§Ü§Ñ§Ý§î§ß§í§Ö §á§Ö§â§Ö§Þ§Ö§ß§ß§í§Ö
 * ---------------------------------------------------------------------------*/
static TBTNACTION btnAct = BTN_DISABLED;
static TBTNEVT btnEvt = BTN_EVT_NONE;

/* -----------------------------------------------------------------------------
 * §®§Ñ§ê§Ú§ß§Ñ §ã§à§ã§ä§à§ñ§ß§Ú§Û §Ü§ß§à§á§Ü§Ú
 * ---------------------------------------------------------------------------*/
typedef enum {
    SM_WAIT_PRESS,
    SM_DEBOUNCE_PRESS,
    SM_WAIT_RELEASE,
    SM_DEBOUNCE_RELEASE,
    SM_WAIT_SECOND_PRESS
} ButtonState;

static ButtonState btnState = SM_WAIT_PRESS;
static uint32_t debounceCounter = 0;
static uint32_t doubleClickTimer = 0;  /* §ä§Ñ§Û§Þ§Ö§â §à§Ø§Ú§Õ§Ñ§ß§Ú§ñ §Ó§ä§à§â§à§Ô§à §ß§Ñ§Ø§Ñ§ä§Ú§ñ */

/* -----------------------------------------------------------------------------
 * §£§ã§á§à§Þ§à§Ô§Ñ§ä§Ö§Ý§î§ß§Ñ§ñ §æ§å§ß§Ü§è§Ú§ñ: §é§ä§Ö§ß§Ú§Ö §ã§à§ã§ä§à§ñ§ß§Ú§ñ §Ü§ß§à§á§Ü§Ú
 * §£§à§Ù§Ó§â§Ñ§ë§Ñ§Ö§ä 1, §Ö§ã§Ý§Ú §Ü§ß§à§á§Ü§Ñ §ß§Ñ§Ø§Ñ§ä§Ñ (§Ñ§Ü§ä§Ú§Ó§ß§í§Û §ß§Ú§Ù§Ü§Ú§Û §å§â§à§Ó§Ö§ß§î)
 * ---------------------------------------------------------------------------*/
static bool Button_IsPressed(void) {
    return !GPIO_ReadInputDataBit(PIN_BTN);
}

/* -----------------------------------------------------------------------------
 * §°§ã§ß§à§Ó§ß§à§Û §à§Ò§â§Ñ§Ò§à§ä§é§Ú§Ü ¡ª §Ó§í§Ù§í§Ó§Ñ§ä§î §Ü§Ñ§Ø§Õ§í§Û 1 §Þ§ã
 * ---------------------------------------------------------------------------*/
void Button_OnTick(void) {
    if (btnAct == BTN_DISABLED) {
        btnState = SM_WAIT_PRESS;
        return;
    }

    bool pressed = Button_IsPressed();

    switch (btnState) {
        case SM_WAIT_PRESS:
            if (pressed) {
                /* §¯§Ñ§Ø§Ñ§ä§Ú§Ö §à§Ò§ß§Ñ§â§å§Ø§Ö§ß§à ¡ª §á§Ö§â§Ö§ç§à§Õ§Ú§Þ §Ü §á§à§Õ§Ñ§Ó§Ý§Ö§ß§Ú§ð §Õ§â§Ö§Ò§Ö§Ù§Ô§Ñ */
                btnState = SM_DEBOUNCE_PRESS;
                debounceCounter = DEBOUNCE_TICK_COUNT;
            }
            break;

        case SM_DEBOUNCE_PRESS:
            if (!pressed) {
                /* §°§ä§á§å§ã§ä§Ú§Ý§Ú §â§Ñ§ß§î§ê§Ö §Ó§â§Ö§Þ§Ö§ß§Ú ¡ª §ã§Ò§â§à§ã */
                btnState = SM_WAIT_PRESS;
            } else {
                if (debounceCounter > 0) {
                    debounceCounter--;
                } else {
                    /* §¥§â§Ö§Ò§Ö§Ù§Ô §á§à§Õ§Ñ§Ó§Ý§Ö§ß, §Ü§ß§à§á§Ü§Ñ §ä§à§é§ß§à §ß§Ñ§Ø§Ñ§ä§Ñ */
                    btnState = SM_WAIT_RELEASE;
                }
            }
            break;

        case SM_WAIT_RELEASE:
            if (!pressed) {
                /* §¬§ß§à§á§Ü§Ñ §à§ä§á§å§ë§Ö§ß§Ñ ¡ª §á§à§Õ§Ñ§Ó§Ý§ñ§Ö§Þ §Õ§â§Ö§Ò§Ö§Ù§Ô §ß§Ñ §à§ä§á§å§ã§Ü§Ñ§ß§Ú§Ö */
                btnState = SM_DEBOUNCE_RELEASE;
                debounceCounter = DEBOUNCE_TICK_COUNT;
            }
            /* §¦§ã§Ý§Ú §Ü§ß§à§á§Ü§Ñ §Ó§ã§× §Ö§ë§× §ß§Ñ§Ø§Ñ§ä§Ñ ¡ª §Ø§Õ§×§Þ §à§ä§á§å§ã§Ü§Ñ§ß§Ú§ñ */
            break;

        case SM_DEBOUNCE_RELEASE:
            if (pressed) {
                /* §³§ß§à§Ó§Ñ §ß§Ñ§Ø§Ñ§Ý§Ú ¡ª §Ó§à§Ù§Þ§à§Ø§ß§à, §Õ§Ó§à§Û§ß§à§Û §Ü§Ý§Ú§Ü */
                btnState = SM_DEBOUNCE_PRESS;
                debounceCounter = DEBOUNCE_TICK_COUNT;
                /* §³§Ò§â§à§ã §ä§Ñ§Û§Þ§Ö§â§Ñ §Ó§ä§à§â§à§Ô§à §Ü§Ý§Ú§Ü§Ñ, §ä.§Ü. §ï§ä§à §å§Ø§Ö §ß§à§Ó§à§Ö §ß§Ñ§Ø§Ñ§ä§Ú§Ö */
                doubleClickTimer = 0;
            } else {
                if (debounceCounter > 0) {
                    debounceCounter--;
                } else {
                    /* §¬§ß§à§á§Ü§Ñ §ä§à§é§ß§à §à§ä§á§å§ë§Ö§ß§Ñ ¡ª §â§Ö§Ô§Ú§ã§ä§â§Ú§â§å§Ö§Þ §à§Õ§Ú§ß§à§é§ß§à§Ö §ß§Ñ§Ø§Ñ§ä§Ú§Ö */
                    if (btnEvt == BTN_EVT_NONE) {
                        btnEvt = BTN_EVT_SINGLE;
                    }
                    /* §©§Ñ§á§å§ã§Ü§Ñ§Ö§Þ §ä§Ñ§Û§Þ§Ö§â §à§Ø§Ú§Õ§Ñ§ß§Ú§ñ §Ó§ä§à§â§à§Ô§à §Ü§Ý§Ú§Ü§Ñ */
                    doubleClickTimer = DOUBLE_CLICK_TIMEOUT_MS;
                    btnState = SM_WAIT_SECOND_PRESS;
                }
            }
            break;

        case SM_WAIT_SECOND_PRESS:
            /* §¨§Õ§×§Þ §Ó§ä§à§â§à§Ö §ß§Ñ§Ø§Ñ§ä§Ú§Ö §Ó §ä§Ö§é§Ö§ß§Ú§Ö DOUBLE_CLICK_TIMEOUT_MS */
            if (doubleClickTimer > 0) {
                doubleClickTimer--;
            } else {
                /* §´§Ñ§Û§Þ§Ñ§å§ä §Ú§ã§ä§×§Ü ¡ª §ã§é§Ú§ä§Ñ§Ö§Þ §à§Õ§Ú§ß§à§é§ß§à§Ö §ß§Ñ§Ø§Ñ§ä§Ú§Ö §à§Ü§à§ß§é§Ñ§ä§Ö§Ý§î§ß§í§Þ */
                btnEvt = BTN_EVT_NONE; /* §å§Ø§Ö §Ò§í§Ý§à §å§ã§ä§Ñ§ß§à§Ó§Ý§Ö§ß§à §Ü§Ñ§Ü SINGLE, §ß§à §Þ§à§Ø§ß§à §ã§Ò§â§à§ã§Ú§ä§î, §Ö§ã§Ý§Ú §ß§å§Ø§ß§à */
                btnState = SM_WAIT_PRESS;
            }

            if (pressed && (doubleClickTimer > 0)) {
                /* §£§ä§à§â§à§Ö §ß§Ñ§Ø§Ñ§ä§Ú§Ö §á§â§Ú§ê§Ý§à §Ó§à§Ó§â§Ö§Þ§ñ ¡ª §Õ§Ó§à§Û§ß§à§Û §Ü§Ý§Ú§Ü */
                btnEvt = BTN_EVT_DOUBLE;
                btnState = SM_WAIT_PRESS;
                doubleClickTimer = 0;
                return; /* §á§â§Ö§â§í§Ó§Ñ§Ö§Þ, §é§ä§à§Ò§í §ß§Ö §à§Ò§â§Ñ§Ò§à§ä§Ñ§ä§î §Ý§Ú§ê§ß§Ö§Ö */
            }
            break;
    }
}

void Button_SetAction(TBTNACTION action) {
    btnAct = action;
    btnEvt = BTN_EVT_NONE;
    btnState = SM_WAIT_PRESS;
    debounceCounter = 0;
    doubleClickTimer = 0;
}

TBTNEVT Button_GetEvt(void) {
    TBTNEVT evt = btnEvt;
    if (evt != BTN_EVT_NONE) {
        btnEvt = BTN_EVT_NONE;  /* §ã§Ò§â§à§ã §ã§à§Ò§í§ä§Ú§ñ */
    }
    return evt;
}
