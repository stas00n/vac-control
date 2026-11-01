#ifndef _BUTTON_H_
#define _BUTTON_H_

#include "ch32v00x_conf.h"
#include "io_conf.h"

/* §´§Ú§á §Õ§Ö§Û§ã§ä§Ó§Ú§ñ §ß§Ñ §Ü§à§ä§à§â§à§Ö §Ü§ß§à§á§Ü§Ñ §Ò§å§Õ§Ö§ä §â§Ö§Ñ§Ô§Ú§â§à§Ó§Ñ§ä§î */
typedef enum {
    BTN_DISABLED = 0,
    BTN_PUSH,
    BTN_HOLD,
    BTN_CLICK,
    BTN_DBLCLICK
}TBTNACTION;


void Button_OnTick(void);
void Button_SetAction(TBTNACTION action);
uint32_t Button_GetEvt();

#endif /* _BUTTON_H_ */