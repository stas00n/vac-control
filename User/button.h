#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>
/* События кнопки (читаются через Button_GetEvt, одно событие за вызов) */
typedef enum {
    BTN_EVT_NONE = 0,
    BTN_EVT_CLICK,   /* нажатие и отпускание короче HOLD_TIME_MS */
    BTN_EVT_DOUBLE,  /* два CLICK с интервалом до DOUBLE_CLICK_TIMEOUT_MS */
    BTN_EVT_HOLD     /* кнопка удержана дольше HOLD_TIME_MS */
} TBTNEVT;

void    Button_Enable(void);   /* разрешить обработку (блокирует до отпускания кнопки) */
void    Button_Disable(void);  /* запретить обработку */
void    Button_OnTick(void);   /* вызывать каждые 1 мс из SysTick */
TBTNEVT Button_GetEvt(void);   /* вернуть и сбросить последнее событие */


#endif /* BUTTON_H */
