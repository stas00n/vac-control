/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH (§Ñ§Õ§Ñ§á§ä§Ú§â§à§Ó§Ñ§ß§à: TBTNEVT §Ú§Ù button.h, §ä§Ñ§Û§Þ§Ö§â §ã§ß§Ñ §Ó IDLE)
 * Version            : V2.0.0
 * Description        : §£§ã§ñ §Ý§à§Ô§Ú§Ü§Ñ §Ó§ß§å§ä§â§Ú switch(opMode).
 *                      §ª§ã§á§à§Ý§î§Ù§å§Ö§ä§ã§ñ TBTNEVT §Ú§Ù button.h.
 *                      §£ IDLE §à§ä§ã§Ý§Ö§Ø§Ú§Ó§Ñ§Ö§ä§ã§ñ §ä§Ñ§Û§Þ§Ö§â §ã§ß§Ñ -> Standby().
 *******************************************************************************/


#include "Init.h"
#include "display.h"
#include "button.h"
#include "SysTick.h"
#include "lpf.h"
#include "io_conf.h"


// §ß§Ö §Ó§í§Ü§Ý§ð§é§Ñ§ä§î SWD §à§ä§Ý§Ñ§Õ§é§Ú§Ü §Ó §â§Ö§Ø§Ú§Þ§Ö §ã§ß§Ñ!!!
#define ENABLE_DEBUG_IN_LOWPOWER_MODE

/* -----------------------------------------------------------------------------
 * §¬§à§ß§ã§ä§Ñ§ß§ä§í
 * ---------------------------------------------------------------------------*/
#define MOTOR_PWM_MIN           84U
#define MOTOR_PWM_MAX           168U
#define MOTOR_PROTECT_TIME_MS   8000U
#define P_DERIVATIVE_LOW_THRESHOLD  5
#define IDLE_SLEEP_TIMEOUT_MS       10000U  /* §´§Ñ§Û§Þ§Ñ§å§ä §Õ§à §á§Ö§â§Ö§ç§à§Õ§Ñ §Ó Standby */

#define DISP_UPDATE_PERIOD_MS      500U
#define TYP_IDLE_VAC_VALUE      2048U
#define TYP_MAX_VAC_VALUE        920U

/* -----------------------------------------------------------------------------
 * §±§Ö§â§Ö§é§Ú§ã§Ý§Ö§ß§Ú§ñ §Ú §ã§ä§â§å§Ü§ä§å§â§í
 * ---------------------------------------------------------------------------*/
typedef enum {
    MODE_IDLE,
    MODE_PUMP,
    MODE_TRACK,
    MODE_HOLD,
    MODE_WAKEUP
} OpMode_t;

static OpMode_t opMode = MODE_IDLE;

/* §´§Ñ§Û§Þ§Ö§â§í: §ç§â§Ñ§ß§Ú§Þ ?§ã§Ü§à§Ý§î§Ü§à §à§ã§ä§Ñ§Ý§à§ã§î? §Ó §Þ§ã */
static struct {
    uint32_t toSleep;           /* §Õ§à §ã§ß§Ñ (§ä§à§Ý§î§Ü§à §Õ§Ý§ñ IDLE) */
    uint32_t motorProtect;      /* §Ù§Ñ§ë§Ú§ä§Ñ §Þ§à§ä§à§â§Ñ */
    uint32_t dispUpdate;        /* §ä§Ñ§Û§Þ§Ö§â §à§Ò§ß§à§Ó§Ý§Ö§ß§Ú§ñ §Õ§Ú§ã§á§Ý§Ö§ñ */
    uint32_t delay;             /* §Õ§Ý§ñ §Ò§Ý§à§Ü§Ú§â§å§ð§ë§Ö§Û §Ù§Ñ§Õ§Ö§â§Ø§Ü§Ú */
    uint32_t task_500ms;        /* §Õ§Ý§ñ §Ù§Ñ§Õ§Ñ§é§Ú §Ü§Ñ§Ø§Õ§í§Ö 500 §Þ§ã */
    uint32_t flag_500ms;
} timers;

/* §±§à§â§à§Ô§Ú §Ó§Ñ§Ü§å§å§Þ§Ñ */
static struct {
    uint16_t in_idle;
    uint16_t in_max;
    uint16_t thres60;
    uint16_t thres80;
} vac_values;

/* §¤§Ý§à§Ò§Ñ§Ý§î§ß§í§Ö §á§Ö§â§Ö§Þ§Ö§ß§ß§í§Ö §ã§à§ã§ä§à§ñ§ß§Ú§ñ */
static uint16_t motorPWM = 0U;
static uint16_t p_sensor_Filtered = 0U;
static int16_t  p_derivative = INT16_MAX;
static uint8_t  vac_percent = 0U;
static uint8_t  dispValue = 0U;
static int buttonWakupLock = 0;


/* -----------------------------------------------------------------------------
 * §±§â§à§ä§à§ä§Ú§á§í
 * ---------------------------------------------------------------------------*/
static void Init_Vac_Values(uint16_t idle, uint16_t max);
static uint8_t Calc_Vac_percent(uint16_t value);
static void Vac_Start(void);
static void Vac_Stop(void);
static void Vac_Release(void);
static void Delay_ms(uint32_t ms);
static void Standby(void);  /* §°§Ò§ì§ñ§Ó§Ý§Ö§ß§Ú§Ö §æ§å§ß§Ü§è§Ú§Ú §á§Ö§â§Ö§ç§à§Õ§Ñ §Ó §ã§à§ß */
static void Task500(void);  /* §Ü§Ñ§Ø§Õ§í§Ö 500 §Þ§ã §Ó§í§á§à§Ý§ß§ñ§Ö§Þ */

/* -----------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/
int main(void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();

    /* §ª§ß§Ú§è§Ú§Ñ§Ý§Ú§Ù§Ñ§è§Ú§ñ §ä§Ñ§Û§Þ§Ö§â§à§Ó */
    timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;            /* §£ §ß§Ñ§é§Ñ§Ý§Ö §ß§Ö §ã§á§Ú§Þ */
    timers.motorProtect = 0U;
    timers.dispUpdate = DISP_UPDATE_PERIOD_MS;
    timers.task_500ms = 0U;

    Init_Vac_Values(TYP_IDLE_VAC_VALUE, TYP_MAX_VAC_VALUE);
    
    Init();

    GPIO_SetBits(PIN_EN);           /* §£§Ü§Ý §á§Ú§ä§Ñ§ß§Ú§Ö §ã§ç§Ö§Þ§í */
    
    Delay_ms(200U);                 /* §Ù§Ñ§Õ§Ö§â§Ø§Ü§Ñ §à§Ò§â§Ñ§Ò§à§ä§Ü§Ú §Ü§ß§à§á§Ü§Ú §á§â§Ú §ã§ä§Ñ§â§ä§Ö */
    Button_SetAction(BTN_PUSH);     /* §Ó§Ü§Ý §à§Ò§â§Ñ§Ò§à§ä§Ü§å §Ü§ß§à§á§Ü§Ú */



    while (1) {
        vac_percent = Calc_Vac_percent(p_sensor_Filtered);
        if (timers.flag_500ms) {
            Task500();
            timers.flag_500ms = 0;
        }

        switch (opMode) {
            case MODE_IDLE: {
                /* §Ø§Õ§Ö§Þ §à§ä§á§å§ã§Ü§Ñ§ß§Ú§ñ §Ü§ß§à§á§Ü§Ú §à§ä §Õ§Ý§Ú§ä§Ö§Ý§î§ß§à§Ô§à §ß§Ñ§Ø§Ñ§ä§Ú§ñ §á§â§Ú §á§â§à§Ò§å§Ø§Õ§Ö§ß§Ú§Ú */
                if(buttonWakupLock) {
                    if(GPIO_ReadInputDataBit(PIN_BTN)) {
                        Delay_ms(20);   // §Ü§ß§à§á§Ü§Ñ §à§ä§á§å§ë§Ö§ß§Ñ, §á§Ñ§å§Ù§Ñ §ß§Ñ §Õ§â§Ö§Ò§Ö§Ù§Ô
                        Button_SetAction(BTN_PUSH); // §Ó§Ü§Ý§ð§é§Ñ§Ö§Þ §ê§ä§Ñ§ä§ß§í§Û §à§á§â§à§ã §Ü§ß§à§á§Ü§Ú
                        buttonWakupLock = 0;
                    }
                    break; // §Ò§à§Ý§î§ê§Ö §ß§Ú§é§Ö§Ô§à §ß§Ö §Õ§Ö§Ý§Ñ§Ö§Þ §á§à§Ü§Ñ §Ü§ß§à§á§Ü§Ñ §ß§Ö §à§ä§á§å§ë§Ö§ß§Ñ
                }
                /* §±§à§Ý§å§é§Ñ§Ö§Þ §ã§à§Ò§í§ä§Ú§Ö §Ü§ß§à§á§Ü§Ú (§ä§Ú§á TBTNEVT §à§á§â§Ö§Õ§Ö§Ý§×§ß §Ó button.h) */
                TBTNEVT btnEvt = (TBTNEVT)Button_GetEvt();

                /* §¦§ã§Ý§Ú §Ö§ã§ä§î §­§À§¢§°§¦ §ã§à§Ò§í§ä§Ú§Ö §à§ä §Ü§ß§à§á§Ü§Ú ¡ª §ã§Ò§â§Ñ§ã§í§Ó§Ñ§Ö§Þ §ä§Ñ§Û§Þ§Ö§â §ã§ß§Ñ */
                if (btnEvt != BTN_EVT_NONE) {
                    timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;
                }

                /* §°§Ò§â§Ñ§Ò§à§ä§Ü§Ñ §à§Õ§Ú§ß§à§é§ß§à§Ô§à §Ü§Ý§Ú§Ü§Ñ: §ã§ä§Ñ§â§ä §ß§Ñ§Ü§Ñ§é§Ü§Ú */
                if (btnEvt == BTN_EVT_SINGLE) {
                    opMode = MODE_PUMP;
                    Vac_Start();
                    timers.motorProtect = MOTOR_PROTECT_TIME_MS;
                    break;
                }

                /* §­§à§Ô§Ú§Ü§Ñ §á§Ö§â§Ö§ç§à§Õ§Ñ §Ó §ã§à§ß §á§à §ä§Ñ§Û§Þ§Ö§â§å */
                if (timers.toSleep == 0U) {
                    Standby();
                    /* §±§à§ã§Ý§Ö Standby §®§¬ §å§ç§à§Õ§Ú§ä §Ó §Ô§Ý§å§Ò§à§Ü§Ú§Û §ã§à§ß, §ß§à §Ö§ã§Ý§Ú §á§â§à§Ò§å§Õ§Ú§ä§ã§ñ,
                       §ä§à §ß§å§Ø§ß§à §Ò§å§Õ§Ö§ä §Ü§à§â§â§Ö§Ü§ä§ß§à §Ú§ß§Ú§è§Ú§Ñ§Ý§Ú§Ù§Ú§â§à§Ó§Ñ§ä§î §ã§à§ã§ä§à§ñ§ß§Ú§Ö §Ù§Ñ§ß§à§Ó§à.
                       §¥§Ý§ñ §á§â§à§ã§ä§à§ä§í §ã§é§Ú§ä§Ñ§Ö§Þ, §é§ä§à Standby() §Õ§Ö§Ý§Ñ§Ö§ä §Ó§ã§× §ß§Ö§à§Ò§ç§à§Õ§Ú§Þ§à§Ö. */
                    break; 
                }
                break;
            }

            case MODE_PUMP: {
                /* §¦§ã§Ý§Ú §à§ä§Ü§Ñ§é§Ü§Ñ §Ú§Õ§Ö§ä, §á§Ö§â§Ö§Ù§Ñ§Ô§â§å§Ø§Ñ§Ö§Þ §ä§Ñ§Û§Þ§Ö§â §Ù§Ñ§ë§Ú§ä§í */
                if (p_derivative > P_DERIVATIVE_LOW_THRESHOLD) {
                    timers.motorProtect = MOTOR_PROTECT_TIME_MS;
                }
                /* §¦§ã§Ý§Ú §ä§Ñ§Û§Þ§Ñ§å§ä §á§à §Ù§Ñ§ë§Ú§ä§Ö §Ú§Ý§Ú §ß§Ñ§Ø§Ñ§Ý§Ú §Ü§ß§à§á§Ü§å - §à§ã§ä§Ñ§ß§à§Ó, §ã§Ò§â§à§ã §Ó§Ñ§Ü§å§å§Þ§Ñ, §Ó§í§ç§à§Õ §Ó IDLE */
                if ((timers.motorProtect == 0U) || (Button_GetEvt() == BTN_EVT_SINGLE)) {
                    Vac_Stop();
                    opMode = MODE_IDLE;
                    timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;
                    break;
                }

                if ((p_sensor_Filtered < vac_values.thres60) &&
                    (p_derivative < P_DERIVATIVE_LOW_THRESHOLD)) {
                    /* §°§ä§Ü§Ñ§é§Ü§Ñ §Ù§Ñ§Ó§Ö§â§ê§Ö§ß§Ñ ¡ª §á§Ö§â§Ö§ç§à§Õ§Ú§Þ §Ó HOLD */
                    Init_Vac_Values(TYP_IDLE_VAC_VALUE, p_sensor_Filtered);
                    Vac_Stop();
                    opMode = MODE_HOLD;
                    break;
                }
                break;
            }

            case MODE_HOLD: {
                /* §£ HOLD: §ã§Ý§Ö§Õ§Ú§Þ §Ù§Ñ §á§à§â§à§Ô§à§Þ ¡ª §Ö§ã§Ý§Ú §Õ§Ñ§Ó§Ý§Ö§ß§Ú§Ö §Õ§à§ã§ä§Ñ§ä§à§é§ß§à §Ó§í§ã§à§Ü§à§Ö, §Ú§Õ§×§Þ §Ó TRACK */
                if (p_sensor_Filtered <= vac_values.thres80) {
                    Vac_Stop();
                    opMode = MODE_TRACK;
                    break;
                }
                /* §´§Ñ§Û§Þ§Ñ§å§ä §á§à §Ù§Ñ§ë§Ú§ä§Ö - §à§ã§ä§Ñ§ß§à§Ó, §ã§Ò§â§à§ã §Ó§Ñ§Ü§å§å§Þ§Ñ, §Ó§í§ç§à§Õ §Ó IDLE */
                if (timers.motorProtect == 0U) {
                    Vac_Stop();
                    Vac_Release();
                    opMode = MODE_IDLE;
                    timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;
                    break;
                }
                /* §¬§ß§à§á§Ü§Ú §Ó HOLD §ß§Ö §à§Ò§â§Ñ§Ò§Ñ§ä§í§Ó§Ñ§Ö§Þ (§Þ§à§Ø§ß§à §Õ§à§Ò§Ñ§Ó§Ú§ä§î, §Ö§ã§Ý§Ú §ß§å§Ø§ß§à) */
                break;
            }

            case MODE_TRACK: {
                TBTNEVT btnEvt = (TBTNEVT)Button_GetEvt();

                /* §£ TRACK: §à§Õ§Ú§ß§à§é§ß§í§Û §Ú§Ô§ß§à§â§Ú§â§å§Ö§Þ, §Õ§Ó§à§Û§ß§à§Û ¡ª §ã§Ò§â§à§ã §Ó§Ñ§Ü§å§å§Þ§Ñ */
                if (btnEvt == BTN_EVT_DOUBLE) {
                    Vac_Release();
                    opMode = MODE_IDLE;
                    timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;
                    break;
                }

                /* §­§à§Ô§Ú§Ü§Ñ §Õ§à§Ü§Ñ§é§Ü§Ú: §Ö§ã§Ý§Ú §å§á§Ñ§Ý§à §ß§Ú§Ø§Ö 60% ¡ª §Ó§Ü§Ý§ð§é§Ñ§Ö§Þ §Þ§à§ä§à§â §Ú §Ú§Õ§×§Þ §Ó HOLD */
                if (p_sensor_Filtered > vac_values.thres60) {
                    Vac_Start();
                    timers.motorProtect = MOTOR_PROTECT_TIME_MS;
                    opMode = MODE_HOLD;
                    break;
                }
                break;
            }
            
            case MODE_WAKEUP:
                /* §°§Ø§Ú§Õ§Ñ§Ö§Þ §å§Õ§Ö§â§Ø§Ñ§ß§Ú§ñ §Ü§ß§à§á§Ü§Ú 1 §ã */
                Delay_ms(50);   // §á§Ñ§å§Ù§Ñ §á§à§Õ§Ñ§Ó§Ý§Ö§ß§Ú§ñ §Õ§â§Ö§Ò§Ö§Ù§Ô§Ñ
                timers.delay = 950;
                int btnHold = 1;
                while(timers.delay) {
                    if(GPIO_ReadInputDataBit(PIN_BTN)) {
                        btnHold = 0;
                        break;  /* while break */
                    }
                }
                if(!btnHold) {
                    /* §Ü§ß§à§á§Ü§å §à§ä§á§å§ã§ä§Ú§Ý§Ú §â§Ñ§ß§î§ê§Ö 1 §ã, §Ù§Ñ§ã§í§á§Ñ§Ö§Þ §à§Ò§â§Ñ§ä§ß§à */
                    Standby();
                    break;
                }
                else {
                    Init(); // §â§Ö§Ú§ß§Ú§è§Ú§Ñ§Ý§Ú§Ù§Ñ§è§Ú§ñ §á§Ö§â§Ú§æ§Ö§â§Ú§Ú §Ú SysTick
                    GPIO_SetBits(PIN_EN);   // §£§Ü§Ý §á§Ú§ä§Ñ§ß§Ú§ñ
                    timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;
                    buttonWakupLock = 1;
                    opMode = MODE_IDLE;
                }
            break;

            default:
                break;
        }
    }
}

/* -----------------------------------------------------------------------------
 * §±§â§Ö§â§í§Ó§Ñ§ß§Ú§ñ
 * ---------------------------------------------------------------------------*/
void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick_Handler(void) {
    /* §Ó §â§Ö§Ø§Ú§Þ§Ö §á§â§à§Ò§å§Ø§Õ§Ö§ß§Ú§ñ §â§Ñ§Ò§à§ä§Ñ§Ö§ä §ä§à§Ý§î§Ü§à §ä§Ñ§Û§Þ§Ö§â §Õ§Ý§ñ Delay_ms() */
    if(opMode == MODE_WAKEUP) {
        if (timers.delay > 0) {
            timers.delay--;
        }
        SysTick->SR = 0;
        return;
    }

    /* §³§ä§Ñ§â§ä§å§Ö§Þ §á§â§Ö§à§Ò§â§Ñ§Ù§à§Ó§Ñ§ß§Ú§Ö */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    /* §±§Ý§Ñ§Ó§ß§í§Û §â§Ñ§Ù§Ô§à§ß §Þ§à§ä§à§â§Ñ */
    if (motorPWM > 0U && motorPWM < MOTOR_PWM_MAX) {
        motorPWM++;
        TIM2->CH4CVR = motorPWM;
    } else if (motorPWM == 0U) {
        TIM2->CH4CVR = 0U;
    }

    Display_Refresh(dispValue);

    /* §¥§Ö§Ü§â§Ö§Þ§Ö§ß§ä §ä§Ñ§Û§Þ§Ö§â§à§Ó §Ü§Ñ§Ø§Õ§å§ð §Þ§Ú§Ý§Ý§Ú§ã§Ö§Ü§å§ß§Õ§å */
    if (timers.delay > 0U) timers.delay--;
    if (timers.toSleep > 0U) timers.toSleep--;
    if (timers.motorProtect > 0U) timers.motorProtect--;
    if (timers.dispUpdate > 0U) timers.dispUpdate--;

    /* §ä§Ñ§Û§Þ§Ö§â§í §Ù§Ñ§Õ§Ñ§é */
    if (timers.task_500ms > 0U ) {
        timers.task_500ms--;
    }
    else {
        timers.flag_500ms = 1;  // §Ó§Ù§Ó§à§Õ§Ú§Þ §æ§Ý§Ñ§Ô §ß§Ñ §Ú§ã§á§à§Ý§ß§Ö§ß§Ú§Ö
        timers.task_500ms = 500U;   // §á§Ö§â§Ö§Ù§Ñ§Ô§â§å§Ø§Ñ§Ö§Þ §ä§Ñ§Û§Þ§Ö§â §Ù§Ñ§Õ§Ñ§é§Ú
    }

    /* §°§Ò§ß§à§Ó§Ý§Ö§ß§Ú§Ö §à§ä§à§Ò§â§Ñ§Ø§Ñ§Ö§Þ§à§Ô§à §Ù§ß§Ñ§é§Ö§ß§Ú§ñ §á§à §ä§Ñ§Û§Þ§Ö§â§å */
    if (timers.dispUpdate == 0U) {
        dispValue = vac_percent;
        timers.dispUpdate = DISP_UPDATE_PERIOD_MS;
    }

    Button_OnTick();
    SysTick->SR = 0U;
}

void ADC1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void ADC1_IRQHandler(void) {
    p_sensor_Filtered = LPF(ADC1->RDATAR);
}

/* -----------------------------------------------------------------------------
 * §£§ã§á§à§Þ§à§Ô§Ñ§ä§Ö§Ý§î§ß§í§Ö §æ§å§ß§Ü§è§Ú§Ú
 * ---------------------------------------------------------------------------*/
void Delay_ms(uint32_t ms) {
    timers.delay = ms;
    while(timers.delay) {
        ;
    }
}

static void Vac_Start(void) {
    /* §³§ß§Ñ§é§Ñ§Ý§Ñ §ã§ä§Ñ§â§ä §Þ§à§ä§à§â§Ñ */
    motorPWM = MOTOR_PWM_MIN;
    TIM2->CH4CVR = motorPWM;
    TIM_CtrlPWMOutputs(TIM2, ENABLE);
    /* §Ù§Ñ§Õ§Ö§â§Ø§Ü§Ñ */
    Delay_ms(200U);
    /* §á§à§ä§à§Þ §à§ä§Ü§â§í§Ó§Ñ§Ö§Þ §Ü§Ý§Ñ§á§Ñ§ß #1 */
    GPIO_SetBits(PIN_VAL1);
}

static void Vac_Stop(void) {
    /* §©§Ñ§Ü§â§í§Ó§Ñ§Ö§Þ §Ü§Ý§Ñ§á§Ñ§ß #1 */
    GPIO_ResetBits(PIN_VAL1);
    Delay_ms(200U);
    /* §±§à§ä§à§Þ §Ó§í§Ü§Ý. §Þ§à§ä§à§â */
    motorPWM = 0U;
    TIM2->CH4CVR = 0U;
    TIM_CtrlPWMOutputs(TIM2, DISABLE);
}

static void Vac_Release(void) {
    /* §°§ä§Ü§â§í§Ó§Ñ§Ö§Þ §Ü§Ý§Ñ§á§Ñ§ß #2 §ß§Ñ 1 §ã */
    GPIO_SetBits(PIN_VAL2);
    Delay_ms(1000U);
    GPIO_ResetBits(PIN_VAL2);
}



static void Init_Vac_Values(uint16_t idle, uint16_t max) {
    vac_values.in_idle = idle;
    vac_values.in_max = max;
    uint16_t fullscale = idle - max;
    if (fullscale == 0) fullscale = 1U;
    vac_values.thres60 = idle - (fullscale * 60) / 100;
    vac_values.thres80 = idle - (fullscale * 80) / 100;
}

static uint8_t Calc_Vac_percent(uint16_t value) {
    int16_t reading = vac_values.in_idle - value;
    uint16_t fullscale = vac_values.in_idle - vac_values.in_max;
    if (fullscale == 0) fullscale = 1U;
    int32_t percent = reading * 100L;
    percent /= fullscale;
    if (percent < 10) percent = 0;
    else if (percent > 99) percent = 99;
    return (uint8_t)percent;
}


static void Task500(void) {
    // §ã§é§Ú§ä§Ñ§Ö§Þ §á§â§à§Ú§Ù§Ó§à§Õ§ß§å§ð §Õ§Ñ§Ó§Ý§Ö§ß§Ú§ñ dP/dt, §à§ß§Ñ §ß§å§Ø§ß§Ñ §Ó §â§Ö§Ø§Ú§Þ§Ö PUMP!
    static uint32_t p_prev = 0;
    static int first = 1;

    if (first) {
        p_prev = p_sensor_Filtered;
        first = 0;
        p_derivative = INT16_MAX;  // §à§ã§ä§Ñ§Ó§Ý§ñ§Ö§Þ §ä§Ó§à§× §Ù§Ñ§ë§Ú§ä§ß§à§Ö §Ù§ß§Ñ§é§Ö§ß§Ú§Ö
    } 
    else {
        int32_t diff = (int32_t)p_prev - (int32_t)p_sensor_Filtered;
        // §°§Ô§â§Ñ§ß§Ú§é§Ú§Ó§Ñ§Ö§Þ, §é§ä§à§Ò§í §ß§Ö §Ò§í§Ý§à §á§Ö§â§Ö§á§à§Ý§ß§Ö§ß§Ú§ñ §á§â§Ú §á§â§Ú§ã§Ó§Ñ§Ú§Ó§Ñ§ß§Ú§Ú §Ó int16_t
        if (diff > INT16_MAX) diff = INT16_MAX;
        if (diff < INT16_MIN) diff = INT16_MIN;
        p_derivative = (int16_t)diff;
        p_prev = p_sensor_Filtered;
    }
}


/* -----------------------------------------------------------------------------
 * §¶§å§ß§Ü§è§Ú§ñ §á§Ö§â§Ö§ç§à§Õ§Ñ §Ó §â§Ö§Ø§Ú§Þ §ã§ß§Ñ (§Õ§à§Ý§Ø§ß§Ñ §Ò§í§ä§î §â§Ö§Ñ§Ý§Ú§Ù§à§Ó§Ñ§ß§Ñ §Ó §à§ä§Õ§Ö§Ý§î§ß§à§Þ §æ§Ñ§Û§Ý§Ö 
 * §Ú§Ý§Ú §Ù§Õ§Ö§ã§î, §Ö§ã§Ý§Ú §Ý§à§Ô§Ú§Ü§Ñ §á§â§à§ã§ä§Ñ§ñ)
 * ---------------------------------------------------------------------------*/
void Standby(void) {
    // §°§ã§ä§Ñ§ß§Ñ§Ó§Ý§Ú§Ó§Ñ§Ö§Þ SysTick, §é§ä§à§Ò§í §á§à§Ô§Ñ§ã§Ú§ä§î §Õ§Ú§ã§á§Ý§Ö§Û
    NVIC_DisableIRQ(SysTick_IRQn);
    SysTick->CTLR &= ~STK_CTLR_STE;
    // §£§í§Ü§Ý §á§Ú§ä§Ñ§ß§Ú§Ö §Ú §ã§Ö§ß§ã§à§â
    GPIO_ResetBits(PIN_EN);
    // §£§í§Ü§Ý §Õ§Ú§ã§á§Ý§Ö§Û
    GPIO_ResetBits(PIN_DISP_0);
    GPIO_ResetBits(PIN_DISP_1);

    // §£§í§Ü§Ý §°§µ
    OPA_Cmd(DISABLE);
    
    // §£§í§Ü§Ý §¡§¸§±
    ADC_Cmd(ADC1, DISABLE);

    // §¬§à§ß§æ§Ú§Ô §ã§à§Ò§í§ä§Ú§ñ §Õ§Ý§ñ §á§â§à§Ò§å§Ø§Õ§Ö§ß§Ú§ñ
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    EXTI_InitTypeDef EXTI_InitStructure = {0};
    EXTI_InitStructure.EXTI_Line = EXTI_Line0;  // §¬§ß§à§á§Ü§Ñ §ß§Ñ PD0
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    // §¬§à§ß§æ§Ú§Ô§å§â§Ú§â§å§Ö§Þ §ß§Ñ GPIOD
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOD, GPIO_PinSource0);

    // §µ§ã§í§á§Ý§ñ§Ö§Þ
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

#ifdef ENABLE_DEBUG_IN_LOWPOWER_MODE
#define PFIC_SCTLR_DEEPSLEEP    ((uint32_t)4)
/*!< Low-power mode of the control system. 1: DEEPSLEEP 0: SLEEP */
    /* §£§ç§à§Õ §Ó Sleep mode §Ó§Þ§Ö§ã§ä§à Standby, §é§ä§à§Ò§í §à§ã§ä§Ñ§Ý§ã§ñ §â§Ñ§Ò§à§ä§Ñ§ä§î §à§ä§Ý§Ñ§Õ§é§Ú§Ü */
    PWR->CTLR = 0;
    NVIC->SCTLR &= ~PFIC_SCTLR_DEEPSLEEP;
    __WFE();
#else
    PWR_EnterSTANDBYMode(PWR_STANDBYEntry_WFE);
#endif

    // §±§à§ã§Ý§Ö §á§â§à§Ò§å§Ø§Õ§Ö§ß§Ú§ñ:
    Button_SetAction(BTN_DISABLED);
    Init_SysTick();
    opMode = MODE_WAKEUP;
    return;
  
}
