#include "Init.h"
#include "ch32v00x_conf.h"
#include "io_conf.h"


static void Init_IO(void);
static void Init_TIM(void);
static void Init_SysTick(void);
static void Init_ADC(void);


void Init() {
    Init_IO();
    //Init_TIM();
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

    /* Вход PPM, TIM2_CH1 (PC1) */
    //gpio.GPIO_Mode = GPIO_Mode_IPD;
    //GPIO_Init_Single(&gpio, PIN_INPUT, 0);
    /* TIM2_RM[1:0] 10: Partial mapping 2(CH1/ETR/PC1, CH2/PD3, CH3/PC0, CH4/PD7). */
    //GPIO_PinRemapConfig(GPIO_PartialRemap2_TIM2, ENABLE);

    /* Выход тестового сигнала - TIM1_CH2 (PA1) */
    //gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    //GPIO_Init_Single(&gpio, PIN_TEST_OUT, 0);
    /* TIM1_RM[1:0] 00: Default mapping (ETR/PC5, CH1/PD2, CH2/PA1, CH3/PC3, CH4/PC4, BKIN/PC2, CH1N/PD0, CH2N/PA2, CH3N/PD1). */
    
}

void Init_TIM(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* TIM1 CH2 - генератор тестового сигнала PPM */
    TIM_TimeBaseInitTypeDef timebase;
    timebase.TIM_ClockDivision = TIM_CKD_DIV4;
    timebase.TIM_CounterMode = TIM_CounterMode_Up;
    timebase.TIM_RepetitionCounter = 0;
    timebase.TIM_Prescaler = 7;       // 1 MHz clock
    timebase.TIM_Period = 19999;      // 20 ms period
    TIM_TimeBaseInit(TIM1, &timebase);


    TIM_OCInitTypeDef tim_oc;
    TIM_OCStructInit(&tim_oc);

    tim_oc.TIM_OCMode = TIM_OCMode_PWM2;
    tim_oc.TIM_OutputState = TIM_OutputState_Enable;
    tim_oc.TIM_OutputNState =TIM_OutputNState_Disable;
    tim_oc.TIM_Pulse = 2000;
    tim_oc.TIM_OCPolarity = TIM_OCPolarity_Low;
    tim_oc.TIM_OCNPolarity = TIM_OCNPolarity_High;
    /* Канал 2! */
    TIM_OC2Init(TIM1, &tim_oc);
    TIM_CtrlPWMOutputs(TIM1, ENABLE );
    TIM_OC2PreloadConfig( TIM1, TIM_OCPreload_Disable );
    TIM_ARRPreloadConfig( TIM1, ENABLE );
    TIM_Cmd(TIM1, ENABLE); 

    /* TIM2 CH1 - захват входного сигнала  */
    timebase.TIM_Period = 22000 - 1;    // Макс. период на захват +10% (22 мс)
    TIM_TimeBaseInit(TIM2, &timebase);

    /* Захват по отрицательному фронту канала 1 */
    TIM_ICInitTypeDef tim_ic;
    TIM_ICStructInit(&tim_ic);
    tim_ic.TIM_Channel = TIM_Channel_1;
    tim_ic.TIM_ICPolarity = TIM_ICPolarity_Falling;
    tim_ic.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInit(TIM2, &tim_ic);

    /* Сброс по положительному фронту TI1F_ED (доступен только от канала 1) */
    TIM_SelectSlaveMode(TIM2, TIM_SlaveMode_Reset);
    TIM_SelectInputTrigger(TIM2, TIM_TS_TI1F_ED);
    TIM_ETRConfig(TIM2, TIM_ExtTRGPSC_OFF, TIM_ExtTRGPolarity_NonInverted, 0);

    /* Прерывание по захвату и обновлению */
    NVIC_InitTypeDef nvic;
    nvic.NVIC_IRQChannel = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    TIM_ITConfig(TIM2, (TIM_IT_CC1 | TIM_IT_Update), ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

#define STK_CTLR_STE        0x1
#define STK_CTLR_STIE       0x2
#define STK_CTLR_STCLK      0x4
#define STK_CTLR_STRE       0x8
#define STK_CTLR_SWIE       0x80000000

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

  //  ADC_ITConfig(ADC1, ADC_IT_EOC, ENABLE);
    //NVIC_EnableIRQ(ADC_IRQn);
}


