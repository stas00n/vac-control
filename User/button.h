#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

typedef enum {
    BTN_DISABLED = 0,
    BTN_PUSH     = 1
} TBTNACTION;

typedef enum {
    BTN_EVT_NONE      = 0,
    BTN_EVT_SINGLE    = 1,
    BTN_EVT_DOUBLE    = 2
} TBTNEVT;

void Button_OnTick(void);
void Button_SetAction(TBTNACTION action);
TBTNEVT Button_GetEvt(void);

#endif /* BUTTON_H */
