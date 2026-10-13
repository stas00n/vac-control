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

/* Global define */


/* Global Variable */
uint16_t adcData = 0;



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
    //Delay_Init();
    Init();

    GPIO_SetBits(PIN_EN);

    while(1) {
        ADC_SoftwareStartConvCmd(ADC1,ENABLE);
        while((ADC1->STATR & ADC_EOC) == 0) {
            ;
        }
        adcData = ADC1->RDATAR;

        if(GPIO_ReadInputDataBit(PIN_BTN) == 0) {
            GPIO_SetBits(PIN_PUMP);
            GPIO_SetBits(PIN_VAL1);
        }
        else {
            GPIO_ResetBits(PIN_PUMP);
            GPIO_ResetBits(PIN_VAL1);
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

    Display_Refresh(dispValue);
    SysTick->SR = 0;
}

void ADC1_IRQHandler(void) {
    adcData = ADC1->RDATAR;
}