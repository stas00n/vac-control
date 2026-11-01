#include "button.h"

/* §­§à§Ü§Ñ§Ý§î§ß§í§Ö §Þ§Ñ§Ü§â§à§ã§í */
#define DEBOUNCE_TICK_COUNT 10
/*!< §Ü§à§Ý§Ú§é§Ö§ã§ä§Ó§à §ä§Ú§Ü§à§Ó §Õ§Ý§ñ §á§à§Õ§Ñ§Ó§Ý§Ö§ß§Ú§ñ §Õ§â§Ö§Ò§Ö§Ù§Ô§Ñ */

/* §­§à§Ü§Ñ§Ý§î§ß§í§Ö §á§Ö§â§Ö§Þ§Ö§ß§ß§í§Ö */
static TBTNACTION btnAct = BTN_DISABLED; // §±§à §å§Þ§à§Ý§é§Ñ§ß§Ú§ð §Ü§ß§à§á§Ü§Ñ §à§ä§Ü§Ý§ð§é§Ö§ß§Ñ §Õ§à §Ú§ß§Ú§è§Ú§Ñ§Ý§Ú§Ù§Ñ§è§Ú§Ú
static uint32_t btnEvt = 0;         // §¶§Ý§Ñ§Ô §ã§à§Ò§í§ä§Ú§ñ

/* §±§â§Ú§Ó§Ñ§ä§ß§í§Ö §æ-§è§Ú§Ú */
static void PrvPush(void);


/**
 * @brief §±§â§à§è§Ö§ã§ã §Ü§ß§à§á§Ü§Ú.
 *
 * @param none
 * @retval none
 * @note §ß§Ö§à§Ò§ç§à§Õ§Ú§Þ§à §Ó§í§Ù§í§Ó§Ñ§ä§î §Ú§Ù §à§ã§ß§à§Ó§ß§à§Û §á§â§à§Ô§â§Ñ§Þ§Þ§í §á§à §ã§Ú§ã§ä§Ö§Þ§ß§à§Þ§å §ä§Ú§Ü§å (§Ü§Ñ§Ø§Õ§í§Ö 1 §Þ§ã)
 * 
 */
void Button_OnTick(void) {
    switch(btnAct) {
        case BTN_PUSH:
            PrvPush();
        break;

        case BTN_DISABLED:
        /* fall through */
        default:
        break;
    }

}


/**
 * @brief §£§í§Ò§à§â §æ§å§ß§Ü§è§Ú§Ú §Ü§ß§à§á§Ü§Ú
 * 
 * @param action 
 */
void Button_SetAction(TBTNACTION action) {
    btnAct = action;
    btnEvt = 0;
}


/**
 * @brief §±§à§Ý§å§é§Ú§ä§î §ã§à§Ò§í§ä§Ú§Ö §Ü§ß§à§á§Ü§Ú
 * 
 * @retval uint32_t §ã§â§Ñ§Ò§à§ä§Ñ§Ý§Ñ §Ü§ß§à§á§Ü§Ñ §Ú§Ý§Ú §ß§Ö§ä
 */
uint32_t Button_GetEvt() {
    if(btnEvt) {
        btnEvt = 0;     // §ã§Ò§â§à§ã §Ý§à§Ü§Ñ§Ý§î§ß§à§Ô§à §æ§Ý§Ñ§Ô§Ñ §ã§â§Ñ§Ò§à§ä§Ü§Ú
        return 1;       // §Ó§à§Ù§Ó§â§Ñ§ä §ã§â§Ñ§Ò§à§ä§Ü§Ú
    }
    else {
        return 0;
    }
}



/**
 * @brief §°§á§â§à§ã §Ü§ß§à§á§Ü§Ú §ß§Ñ §ã§à§Ò§í§ä§Ú§Ö §ß§Ñ§Ø§Ñ§ä§Ú§ñ
 * 
 * @note §ï§ä§Ñ §æ§å§ß§è§Ú§ñ §å§ã§ä§Ñ§ß§Ñ§Ó§Ý§Ú§Ó§Ñ§Ö§ä §æ§Ý§Ñ§Ô §ã§à§Ò§í§ä§Ú§ñ btnEvt §Ö§ã§Ý§Ú §Ü§ß§à§á§Ü§Ñ §Ò§í§Ý§Ñ §ß§Ñ§Ø§Ñ§ä§Ñ,
 * §á§à§Õ§Ñ§Ó§Ý§ñ§Ö§ä §Õ§â§Ö§Ò§Ö§Ù§Ô §Ü§à§ß§ä§Ñ§Ü§ä§à§Ó §Ú §à§Ø§Ú§Õ§Ñ§Ö§ä §à§ä§á§å§ã§Ü§Ñ§ß§Ú§ñ
 */
static void PrvPush(void) {
    static enum {SM_WAITPUSH, SM_DEBOUNCE1, SM_WAITRELEASE, SM_DEBOUNCE2 } stateMachine  = SM_WAITPUSH; // §æ§Ñ§Ù§Ñ §à§á§â§à§ã§Ñ
    static uint32_t prevState = 0;  // §ã§à§ã§ä§à§ñ§ß§Ú§Ö §Ü§à§ß§ä§Ñ§Ü§ä§Ñ §Ó§à §Ó§â§Ö§Þ§ñ §á§Ö§â§Õ§í§Õ§å§ë§Ö§Ô§à §à§á§â§à§ã§Ñ
    static uint32_t debounceTicks = 0;  // §ã§é§Ö§ä§é§Ú§Ü §á§à§Õ§Ñ§Ó§Ý§Ö§ß§Ú§ñ §Õ§â§Ö§Ò§Ö§Ù§Ô§Ñ
    uint32_t state;                 // §ä§Ö§Ü§å§ë§Ö§Ö §ã§à§ã§ä§à§ñ§ß§Ú§Ö §Ü§à§ß§ä§Ñ§Ü§ä§Ñ


    state = !GPIO_ReadInputDataBit(PIN_BTN);

    if (stateMachine == SM_WAITPUSH) {
        if(!prevState && state) {
            btnEvt = 1;
            prevState = state;
            stateMachine = SM_DEBOUNCE1;
            debounceTicks = DEBOUNCE_TICK_COUNT;
            return;
        }
        prevState = state;
    }
    if (stateMachine == SM_DEBOUNCE1) {
        if(debounceTicks) {
            debounceTicks--;    // §Ø§Õ§Ö§Þ §á§à§Õ§Ñ§Ó§Ý§Ö§ß§Ú§ñ §Õ§â§Ö§Ò§Ö§Ù§Ô§Ñ
            return;
        }
        else {
            stateMachine = SM_WAITRELEASE;
            return;
        }
    }
    if (stateMachine == SM_WAITRELEASE) {
        if(!state) {
            stateMachine = SM_DEBOUNCE2;
            debounceTicks = DEBOUNCE_TICK_COUNT;
            return;
        }
    }
    if (stateMachine == SM_DEBOUNCE2) {
        if(debounceTicks) {
            debounceTicks--;    // §Ø§Õ§Ö§Þ §á§à§Õ§Ñ§Ó§Ý§Ö§ß§Ú§ñ §Õ§â§Ö§Ò§Ö§Ù§Ô§Ñ
            return;
        }
        else {
            stateMachine = SM_WAITPUSH;
            return;
        }
    }

}