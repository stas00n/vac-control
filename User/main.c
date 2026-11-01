/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/12/25
 * Description        : Main program body.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "debug.h"
#include "Init.h"
#include "display.h"
#include "button.h"
#include "SysTick.h"
#include "lpf.h"

/* Private define */
#define MOTOR_PWM_MIN 84
#define MOTOR_PWM_MAX 168
#define MOTOR_PROTECT_TIME_MS 8000
#define P_DERIVATIVE_LOW_THRESHOLD 5    /*!< §©§ß§Ñ§é§Ö§ß§Ú§Ö §á§â§à§Ú§Ù§Ó§à§Õ§ß§à§Û §ã§Ú§Ô§ß§Ñ§Ý§Ñ §Õ§Ñ§ä§é§Ú§Ü§Ñ, §Ü§à§ä§à§â§à§Ö §ã§é§Ú§ä§Ñ§Ö§Þ §à§Ü§à§Ý§à§ß§å§Ý§Ö§Ó§í§Þ */
#define IDLE_SLEEP_TIMEOUT_MS 10000     /*!< §ä§Ñ§Û§Þ§Ñ§å§ä §Õ§à §ã§ß§Ñ §Ó §â§Ö§Ø§Ú§Þ§Ö §à§Ø§Ú§Õ§Ñ§ß§Ú§ñ - §á§à §´§© - 5 §Þ§Ú§ß§å§ä */
#define TYP_IDLE_VAC_VALUE 2048u
#define TYP_MAX_VAC_VALUE 920u
/* Global Variable */


/* Private variables */
static enum {MODE_IDLE= 0, MODE_PUMP, MODE_TRACK, MODE_HOLD, MODE_RELEASE} opMode = MODE_IDLE;
static struct {
    uint32_t delay;
    uint32_t toSleep;
    uint32_t motorProtect;
    uint32_t t100ms;
    uint32_t f100ms : 1;
} timers;

/* §©§ß§Ñ§é§Ö§ß§Ú§ñ §ã§Ú§Ô§ß§Ñ§Ý§Ñ §ã §Õ§Ñ§ä§é§Ú§Ü§Ñ §Ó§Ñ§Ü§å§å§Þ§Ñ */
static struct {
    uint16_t in_idle;  /* §á§â§Ú §Ñ§ä§Þ §Õ§Ñ§Ó§Ý§Ö§ß§Ú§Ú */
    uint16_t in_max;   /* §á§â§Ú §Þ§Ñ§Ü§ã §â§Ñ§Ù§â§Ö§Ø§Ö§ß§Ú§Ú */
    uint16_t thres20;   /* §Ö§ã§ä§î §ß§Ö§Ò§à§Ý§î§ê§à§Û §Ó§Ñ§Ü§å§å§Þ (20% §Þ§Ñ§Ü§ã§Ú§Þ§Ñ§Ý§î§ß§à§Ô§à)*/
    uint16_t thres60;   /* §á§à§â§à§Ô §Ó§Ü§Ý§ð§é§Ö§ß§Ú§ñ §á§à§Õ§Ü§Ñ§é§Ü§Ú 60% */
    uint16_t thres80;   /* §á§à§â§à§Ô §à§ä§Ü§Ý§ð§é§Ö§ß§Ú§ñ §á§à§Õ§Ü§Ñ§é§Ü§Ú 80% */
} vac_values;

static uint16_t motorPWM = 0;

static uint16_t psensorFilter = 0;
static int16_t p_derivative = 0;
static uint8_t vac_percent;

int16_t pd[256] = {};
uint8_t pdcnt = 0;



/* Private func prototypes */
static void Delay_ms(uint32_t ms);
static void Task_100(void);
static void Standby(void);
static void Motor_Start(void);
static void Motor_Stop(void);
static void On_Mode_Pump(void);
static void Init_Vac_Values(uint16_t idle, uint16_t max);
static uint8_t Calc_Vac_percent(uint16_t value);

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void) {


    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Init();
    GPIO_SetBits(PIN_EN);

    // §Ú§ß§Ú§è§Ú§Ñ§Ý§Ú§Ù§Ú§â§å§Ö§Þ §á§à§â§à§Ô§Ú §Ó§Ñ§Ü§å§å§Þ§Ñ §ä§Ú§á§à§Ó§í§Þ§Ú §Ù§ß§Ñ§é§Ö§ß§Ú§ñ§Þ§Ú
    Init_Vac_Values(TYP_IDLE_VAC_VALUE, TYP_MAX_VAC_VALUE);

    // §©§Ñ§Õ§Ö§â§Ø§Ü§Ñ §Õ§Ý§ñ §Ù§Ñ§â§ñ§Õ§Ñ §Ü§à§ß§Õ§Ö§ß§ã§Ñ§ä§à§â§Ñ §Ü§ß§à§á§Ü§Ú §á§â§Ú §Ó§Ü§Ý. §á§Ú§ä§Ñ§ß§Ú§ñ, §é§ä§à§Ò§í §ß§Ö §Ò§í§Ý§à §Ý§à§Ø§ß§à§Ô§à §ã§â§Ñ§Ò§Ñ§ä§í§Ó§Ñ§ß§Ú§ñ.
    Delay_ms(200);  
    Button_SetAction(BTN_PUSH);


    while(1) {
        

        if(timers.f100ms) {
            timers.f100ms = 0;
            timers.t100ms = 500;
            Task_100();
        }

        vac_percent = Calc_Vac_percent(psensorFilter);
        

        switch(opMode) {
            case MODE_IDLE:
            if(Button_GetEvt()) {
                opMode = MODE_PUMP;
                Motor_Start();
                Delay_ms(200);
                GPIO_SetBits(PIN_VAL1);
                pdcnt = 0;
                // §©§Ñ§Ô§â§å§Ø§Ñ§Ö§Þ §ä§Ñ§Û§Þ§Ö§â §Ù§Ñ§ë§Ú§ä§í
                timers.motorProtect = MOTOR_PROTECT_TIME_MS;
                // §°§Ò§ß§à§Ó§Ý§ñ§Ö§Þ §á§à§â§à§Ô§Ú
                Init_Vac_Values(TYP_IDLE_VAC_VALUE, TYP_MAX_VAC_VALUE);
            }
            break;

            case MODE_PUMP:
                On_Mode_Pump();
            break;

            case MODE_TRACK:
            // §ß§Ñ§Ø§Ñ§Ý§Ú §Ü§ß§à§á§Ü§å, §ã§Ò§â§à§ã §Ó§Ñ§Ü§å§å§Þ§Ñ
            if(Button_GetEvt()) {
                GPIO_SetBits(PIN_VAL2);
                Delay_ms(1000);
                GPIO_ResetBits(PIN_VAL2);
                opMode = MODE_IDLE;
                timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;
                break;
            }
            // §³§Ý§Ö§Õ§Ú§Þ §Ù§Ñ §ß§Ú§Ø§ß§Ú§Þ §á§à§â§à§Ô§à§Þ §Õ§Ñ§Ó§Ý§Ö§ß§Ú§ñ
            if(psensorFilter > vac_values.thres60) {
                // §£§Ü§Ý§ð§é§Ñ§Ö§Þ §Õ§à§Ü§Ñ§é§Ü§å
                Motor_Start();
                Delay_ms(200);
                GPIO_SetBits(PIN_VAL1);
                // §©§Ñ§Ô§â§å§Ø§Ñ§Ö§Þ §ä§Ñ§Û§Þ§Ö§â §Ù§Ñ§ë§Ú§ä§í
                timers.motorProtect = MOTOR_PROTECT_TIME_MS;
                // §±§Ö§â§Ö§ç§à§Õ§Ú§Þ §Ó §â§Ö§Ø§Ú§Þ §Õ§à§Ü§Ñ§é§Ü§Ú
                opMode = MODE_HOLD;
            }
            break;

            case MODE_HOLD:
            if(psensorFilter <= vac_values.thres80) {
                // §©§Ñ§Ü§â§í§Ó§Ñ§Ö§Þ §Ü§Ý§Ñ§á§Ñ§ß, §Ù§Ñ§ä§Ö§Þ §Ó§í§Ü§Ý §Þ§à§ä§à§â
                GPIO_ResetBits(PIN_VAL1);
                Delay_ms(200);
                Motor_Stop();
                // §Ó§à§Ù§Ó§â§Ñ§ë§Ñ§Ö§Þ§ã§ñ §Ó §â§Ö§Ø§Ú§Þ §ã§Ý§Ö§Ø§Ö§ß§Ú§ñ
                opMode = MODE_TRACK;
            }

            default:
            break;
        }



    }
}

void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick_Handler(void) {
    ADC_SoftwareStartConvCmd(ADC1,ENABLE);

    const uint16_t dispUpdTime_ms = 500;
    static uint16_t dispUpdTimer = dispUpdTime_ms;
    static uint8_t dispValue = 0;
    if(--dispUpdTimer == 0) {
        dispUpdTimer = dispUpdTime_ms;
        dispValue = vac_percent;//(psensorFilter / 40);
    }
    if(timers.delay) {
        timers.delay--;
    }

    if(timers.toSleep) {
        --timers.toSleep;
    }

    if(timers.motorProtect) {
        --timers.motorProtect;
    }
    if(timers.t100ms) {
        --timers.t100ms;
    }
    else {
        timers.f100ms = 1;
    }


    // §â§Ñ§Ù§Ô§à§ß §Þ§à§ä§à§â§Ñ §Ö§ã§Ý§Ú §Ó§Ü§Ý§ð§é§Ö§ß
    if(motorPWM && (motorPWM < MOTOR_PWM_MAX)) {
        motorPWM++;
    }
    TIM2->CH4CVR = motorPWM;

    Display_Refresh(dispValue);
    Button_OnTick();
    SysTick->SR = 0;
}

void ADC1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void ADC1_IRQHandler(void) {
    psensorFilter = LPF(ADC1->RDATAR);
}


void Delay_ms(uint32_t ms) {
    timers.delay = ms;
    while(timers.delay) {
        ;
    }
}


void Task_100(void) {
    static uint32_t p_prev = 0;
    p_derivative = p_prev - psensorFilter;
    p_prev = psensorFilter;
    pd[pdcnt++] = p_derivative;

}

void Standby(void) {
    // §°§ã§ä§Ñ§ß§Ñ§Ó§Ý§Ú§Ó§Ñ§Ö§Þ SysTick, §é§ä§à§Ò§í §á§à§Ô§Ñ§ã§Ú§ä§î §Õ§Ú§ã§á§Ý§Ö§Û
    SysTick->CTLR &= ~STK_CTLR_STE;
    // §£§í§Ü§Ý §á§Ú§ä§Ñ§ß§Ú§Ö §Ú §ã§Ö§ß§ã§à§â
    GPIO_ResetBits(PIN_EN);
    // §£§í§Ü§Ý §Õ§Ú§ã§á§Ý§Ö§Û
    GPIO_ResetBits(PIN_DISP_0);
    GPIO_ResetBits(PIN_DISP_1);
    // §¬§à§ß§æ§Ú§Ô §ã§à§Ò§í§ä§Ú§ñ §Õ§Ý§ñ §á§â§à§Ò§å§Ø§Õ§Ö§ß§Ú§ñ
    EXTI_InitTypeDef EXTI_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    EXTI_InitStructure.EXTI_Line = EXTI_Line0;  // §¬§ß§à§á§Ü§Ñ §ß§Ñ PD0
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    // §¬§à§ß§æ§Ú§Ô§å§â§Ú§â§å§Ö§Þ §ß§Ñ GPIOD
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOD, GPIO_PinSource0);

    // §£§í§Ü§Ý §°§µ
    OPA_Cmd(DISABLE);
    GPIO_InitTypeDef gpio;
    // §£§ç§à§Õ§í §°§µ §à§ä§Ü§Ý§ð§é§Ñ§Ö§Þ
    //gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    //gpio.GPIO_Pin = (GPIO_Pin_0 | GPIO_Pin_1);
    //GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_All;
    gpio.GPIO_Mode = GPIO_Mode_IPD;

    GPIO_Init(GPIOA, &gpio);
    GPIO_Init(GPIOC, &gpio);
    //GPIO_Init(GPIOD, &gpio);
    ADC_Cmd(ADC1, DISABLE);


    // §µ§ã§í§á§Ý§ñ§Ö§Þ
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_EnterSTANDBYMode(PWR_STANDBYEntry_WFE);

    SysTick->CTLR |= STK_CTLR_STE;
    GPIO_SetBits(PIN_EN);
}


static void Motor_Start(void) {
    motorPWM = MOTOR_PWM_MIN;
    TIM2->CH4CVR = motorPWM;
    TIM_CtrlPWMOutputs(TIM2, ENABLE);
}

static void Motor_Stop(void) {
    motorPWM = 0;
    TIM2->CH4CVR = 0;
    TIM_CtrlPWMOutputs(TIM2, DISABLE);
}


static void On_Mode_Pump(void) {
    // §ã§Ý§Ö§Õ§Ú§Þ §Ù§Ñ §á§â§à§Ú§Ù§Ó§à§Õ§ß§à§Û §ã§Ú§Ô§ß§Ñ§Ý§Ñ §Õ§Ñ§ä§é§Ú§Ü§Ñ
    if(p_derivative > P_DERIVATIVE_LOW_THRESHOLD) {
        // §Ö§ã§Ý§Ú §Ü§Ñ§é§Ñ§Ö§ä, §á§Ö§â§Ö§Ù§Ñ§Ô§â§å§Ø§Ñ§Ö§Þ §ä§Ñ§Û§Þ§Ö§â §Ù§Ñ§ë§Ú§ä§í §Þ§à§ä§à§â§Ñ
        timers.motorProtect = MOTOR_PROTECT_TIME_MS;
    }

    // §°§ä§Ü§Ý§ð§é§Ö§ß§Ú§Ö §á§à §ç§à§Ý§à§ã§ä§à§Þ§å §ç§à§Õ§å §Ò§à§Ý§Ö§Ö 8 §ã §Ú§Ý§Ú §á§â§Ú§ß§å§Õ§Ú§ä§Ö§Ý§î§ß§à§Ö §á§à §Ü§ß§à§á§Ü§Ö
    if((timers.motorProtect == 0) || Button_GetEvt()) {
        // §£§í§Ü§Ý §Þ§à§ä§à§â §Ú §Ü§Ý§Ñ§á§Ñ§ß
        GPIO_ResetBits(PIN_VAL1);
        Motor_Stop();
        // §³§Ò§â§à§ã §Ó§Ñ§Ü§å§å§Þ§Ñ, §ä.§Ü. §á§à§á§í§ä§Ü§Ñ §à§ä§Ü§Ñ§é§Ü§Ú §ß§Ö§å§Õ§Ñ§é§ß§Ñ§ñ
        GPIO_SetBits(PIN_VAL2);
        Delay_ms(1000);
        GPIO_ResetBits(PIN_VAL2);
        // §±§Ö§â§Ö§Ù§Ñ§Ô§â§å§Ø§Ñ§Ö§Þ §ä§Ñ§Û§Þ§Ö§â §ã§ß§Ñ §Ú §Ó§í§ç§à§Õ§Ú§Þ §Ó §â§Ö§Ø§Ú§Þ §à§Ø§Ú§Õ§Ñ§ß§Ú§ñ
        timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;
        opMode = MODE_IDLE;
        return;
    }

    // §±§Ö§â§Ö§ç§à§Õ §Ó §â§Ö§Ø§Ú§Þ §á§à§Õ§Õ§Ö§â§Ø§Ñ§ß§Ú§ñ §Ö§ã§Ý§Ú §Ó§Ñ§Ü§å§å§Þ §Ö§ã§ä§î §Ú §Ò§à§Ý§î§ê§Ö §ß§Ö §à§ä§Ü§Ñ§é§Ú§Ó§Ñ§Ö§ä§ã§ñ
    if((psensorFilter < vac_values.thres20) && (p_derivative < P_DERIVATIVE_LOW_THRESHOLD)) {
        // §±§Ö§â§ã§é§Ú§ä§í§Ó§Ñ§Ö§Þ §á§à§â§à§Ô§Ú, §ã§é§Ú§ä§Ñ§ñ §ä§Ö§Ü§å§ë§Ö§Ö §Ù§ß§Ñ§é§Ö§ß§Ú§Ö §ã §Õ§Ñ§ä§é§Ú§Ü§Ñ §Þ§Ñ§Ü§ã§Ú§Þ§Ñ§Ý§î§ß§í§Þ §â§Ñ§Ù§â§Ö§Ø§Ö§ß§Ú§Ö§Þ.
        Init_Vac_Values(TYP_IDLE_VAC_VALUE, psensorFilter);
        // §©§Ñ§Ü§â§í§Ó§Ñ§Ö§Þ §Ü§Ý§Ñ§á§Ñ§ß, §Ù§Ñ§ä§Ö§Þ §Ó§í§Ü§Ý §Þ§à§ä§à§â
        GPIO_ResetBits(PIN_VAL1);
        Delay_ms(200);
        Motor_Stop();
        opMode = MODE_HOLD;
        return;
    }
}

static void Init_Vac_Values(uint16_t idle, uint16_t max) {
    vac_values.in_idle = idle;
    vac_values.in_max = max;
    uint16_t fullscale = idle - max;
    vac_values.thres20 = idle - (fullscale * 20) / 100;
    vac_values.thres60 = idle - (fullscale * 60) / 100;
    vac_values.thres80 = idle - (fullscale * 80) / 100;
}

static uint8_t Calc_Vac_percent(uint16_t value) {
    uint16_t fullscale = vac_values.in_idle - vac_values.in_max;
    int16_t reading = vac_values.in_idle - value;
    int32_t percent = reading * 100;
    percent /= fullscale;
    // §®§Ö§ß§î§ê§Ö 10% §ã§é§Ú§ä§Ñ§Ö§Þ §ß§å§Ý§Ö§Þ
    if(percent < 10) {
        percent = 0;
    }
    // §£§Ö§â§ç§ß§Ö§Ö §à§Ô§â§Ñ§ß§Ú§é§Ö§ß§Ú§Ö 99%
    if(percent > 99) {
        percent = 99;
    }

    return (uint8_t)percent;
}