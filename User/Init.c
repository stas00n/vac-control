#include "Init.h"
#include "ch32v00x_conf.h"
#include "io_conf.h"
#include "SysTick.h"


static void Init_IO(void);
static void Init_TIM(void);
static void Init_SysTick(void);
static void Init_ADC(void);

extern uint32_t SystemCoreClock;

void Init() {
    Init_IO();
    Init_TIM();
    Init_ADC();
    Init_SysTick();
}

void Init_IO(void) {
    RCC_APB2PeriphClockCmd((RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO), ENABLE);

    GPIO_InitTypeDef gpio;

    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_30MHz;


    /* Весь GPIOC - выходы на сегментный дисплей */
    gpio.GPIO_Pin = GPIO_Pin_All;
    GPIO_Init(GPIOC, &gpio);

    /*
     * Выходы GPIOD:
     * PD2 - катод индикатора
     * PD3 - клапан 1
     * PD5 - насос
     * PD6 - клапан 2
     * PD7 - включение основного питания
     */
    gpio.GPIO_Pin = (GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_5 |GPIO_Pin_6 | GPIO_Pin_7);
    GPIO_Init(GPIOD, &gpio);

    /* PD4 - аналоговый вход 7 + выход ОУ */
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    gpio.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOD, &gpio);

    /* PD0 - кнопка */
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOD, &gpio);

    /* PA0, PA1 - входы ОУ */
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    gpio.GPIO_Pin = (GPIO_Pin_0 | GPIO_Pin_1);
    GPIO_Init(GPIOA, &gpio);

    /* Включаем ОУ */
    OPA_InitTypeDef opa = {CHP0, CHN0};
    OPA_Init(&opa);
    OPA_Cmd(ENABLE);

    /* PD5 - T2CH4_3 - PWM выход вкл насоса */
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Pin = GPIO_Pin_5;
    GPIO_Init(GPIOD, &gpio);
    /* TIM2_RM[1:0] = 0b11: Complete mapping (CH1/ETR/PC1, CH2/PC7, CH3/PD6, CH4/PD5). */
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM2, ENABLE);

}

void Init_TIM(void) {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* PD5 - T2CH4_3 - PWM выход вкл насоса */
    /* TIM2_RM[1:0] = 0b11: Complete mapping (CH1/ETR/PC1, CH2/PC7, CH3/PD6, CH4/PD5). */
    
    TIM_TimeBaseInitTypeDef timebase;
    timebase.TIM_ClockDivision = TIM_CKD_DIV4;
    timebase.TIM_CounterMode = TIM_CounterMode_Up;
    timebase.TIM_RepetitionCounter = 0;
    timebase.TIM_Prescaler = 9;//SystemCoreClock / 1000000ul - 1;       // 1 MHz TIM2 clock
    timebase.TIM_Period = 240;      // 10 кГц частота ШИМ
    TIM_TimeBaseInit(TIM2, &timebase);


    TIM_OCInitTypeDef tim_oc;
    TIM_OCStructInit(&tim_oc);

    tim_oc.TIM_OCMode = TIM_OCMode_PWM1;
    tim_oc.TIM_OutputState = TIM_OutputState_Enable;
    tim_oc.TIM_OutputNState =TIM_OutputNState_Disable;
    tim_oc.TIM_Pulse = 0;
    tim_oc.TIM_OCPolarity = TIM_OCPolarity_High;
    tim_oc.TIM_OCNPolarity = TIM_OCNPolarity_High;
    /* Канал 4 */
    TIM_OC4Init(TIM2, &tim_oc);
    //TIM_CtrlPWMOutputs(TIM2, ENABLE );
    TIM_OC2PreloadConfig( TIM2, TIM_OCPreload_Disable );
    TIM_ARRPreloadConfig( TIM2, ENABLE );
    TIM_Cmd(TIM2, ENABLE); 
}



void Init_SysTick(void) {
    NVIC_EnableIRQ(SysTick_IRQn);
    SysTick->SR &= ~(1 << 0);
    SysTick->CMP = SystemCoreClock / 1000 - 1;
    SysTick->CNT = 0;
    SysTick->CTLR = (STK_CTLR_STE | STK_CTLR_STIE | STK_CTLR_STCLK | STK_CTLR_STRE);
}


void Init_ADC(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    ADC_InitTypeDef adc;
    ADC_StructInit(&adc);
    adc.ADC_ScanConvMode = ENABLE;
    adc.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;   // Software trigger
    adc.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &adc);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_7, 1, ADC_SampleTime_241Cycles);

    ADC_Cmd(ADC1, ENABLE);

    ADC_ITConfig(ADC1, ADC_IT_EOC, ENABLE);
    NVIC_EnableIRQ(ADC_IRQn);
}


