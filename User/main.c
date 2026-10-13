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

/* Global define */


/* Global Variable */
uint16_t adcData = 0;
volatile uint32_t delayCnt = 0;

/* Private func prototypes */
static void Delay_ms(uint32_t ms);

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void) {
    static enum {MODE_IDLE= 0, MODE_PUMP, MODE_HOLD, MODE_RELEASE} opMode = MODE_IDLE;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Init();

    GPIO_SetBits(PIN_EN);

    while(1) {

        ADC_SoftwareStartConvCmd(ADC1,ENABLE);
        while((ADC1->STATR & ADC_EOC) == 0) {
            ;
        }
        adcData = ADC1->RDATAR;

        

        switch(opMode) {
            case MODE_IDLE:
            if(Button_GetEvt()) {
                opMode = MODE_PUMP;
                GPIO_SetBits(PIN_PUMP);
                Delay_ms(200);
                GPIO_SetBits(PIN_VAL1);
            }
            break;

            case MODE_PUMP:
            if(Button_GetEvt()) {
                opMode = MODE_HOLD;
                // §³§ß§Ñ§é§Ñ§Ý§Ñ §Ù§Ñ§Ü§â§í§Ó§Ñ§Ö§Þ §Ü§Ý§Ñ§á§Ñ§ß, §á§à§ä§à§Þ §Ó§í§Ü§Ý §Þ§à§ä§à§â
                GPIO_ResetBits(PIN_VAL1);
                Delay_ms(200);
                GPIO_ResetBits(PIN_PUMP);
                
            }
            break;

            case MODE_HOLD:
            if(Button_GetEvt()) {
                GPIO_SetBits(PIN_VAL2);
                Delay_ms(1000);
                GPIO_ResetBits(PIN_VAL2);
                opMode = MODE_IDLE;
            }
            break;

            default:
            break;
        }



    }
}

void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick_Handler(void) {
    const uint16_t dispUpdTime_ms = 500;
    static uint16_t dispUpdTimer = dispUpdTime_ms;
    static uint8_t dispValue = 0;
    if(--dispUpdTimer == 0) {
        dispUpdTimer = dispUpdTime_ms;
        dispValue = (adcData/10);
    }
    if(delayCnt) {
        delayCnt--;
    }

    Display_Refresh(dispValue);
    Button_OnTick();
    SysTick->SR = 0;
}

void ADC1_IRQHandler(void) {
    adcData = ADC1->RDATAR;
}


void Delay_ms(uint32_t ms) {
    delayCnt = ms;
    while(delayCnt) {
        ;
    }
}