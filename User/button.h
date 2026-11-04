#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>
/* §³§à§Ò§í§ä§Ú§ñ §Ü§ß§à§á§Ü§Ú (§é§Ú§ä§Ñ§ð§ä§ã§ñ §é§Ö§â§Ö§Ù Button_GetEvt, §à§Õ§ß§à §ã§à§Ò§í§ä§Ú§Ö §Ù§Ñ §Ó§í§Ù§à§Ó) */
typedef enum {
    BTN_EVT_NONE = 0,
    BTN_EVT_CLICK,   /* §ß§Ñ§Ø§Ñ§ä§Ú§Ö §Ú §à§ä§á§å§ã§Ü§Ñ§ß§Ú§Ö §Ü§à§â§à§é§Ö HOLD_TIME_MS */
    BTN_EVT_DOUBLE,  /* §Õ§Ó§Ñ CLICK §ã §Ú§ß§ä§Ö§â§Ó§Ñ§Ý§à§Þ §Õ§à DOUBLE_CLICK_TIMEOUT_MS */
    BTN_EVT_HOLD     /* §Ü§ß§à§á§Ü§Ñ §å§Õ§Ö§â§Ø§Ñ§ß§Ñ §Õ§à§Ý§î§ê§Ö HOLD_TIME_MS */
} TBTNEVT;

void    Button_Enable(void);   /* §â§Ñ§Ù§â§Ö§ê§Ú§ä§î §à§Ò§â§Ñ§Ò§à§ä§Ü§å (§Ò§Ý§à§Ü§Ú§â§å§Ö§ä §Õ§à §à§ä§á§å§ã§Ü§Ñ§ß§Ú§ñ §Ü§ß§à§á§Ü§Ú) */
void    Button_Disable(void);  /* §Ù§Ñ§á§â§Ö§ä§Ú§ä§î §à§Ò§â§Ñ§Ò§à§ä§Ü§å */
void    Button_OnTick(void);   /* §Ó§í§Ù§í§Ó§Ñ§ä§î §Ü§Ñ§Ø§Õ§í§Ö 1 §Þ§ã §Ú§Ù SysTick */
TBTNEVT Button_GetEvt(void);   /* §Ó§Ö§â§ß§å§ä§î §Ú §ã§Ò§â§à§ã§Ú§ä§î §á§à§ã§Ý§Ö§Õ§ß§Ö§Ö §ã§à§Ò§í§ä§Ú§Ö */


#endif /* BUTTON_H */
