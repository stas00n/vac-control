/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH (адаптировано: TBTNEVT из button.h, таймер сна в IDLE)
 * Version            : V2.0.0
 * Description        : Вся логика внутри switch(opMode).
 *                      Используется TBTNEVT из button.h.
 *                      В IDLE отслеживается таймер сна -> Standby().
 *******************************************************************************/


#include "Init.h"
#include "display.h"
#include "button.h"
#include "SysTick.h"
#include "lpf.h"
#include "io_conf.h"
#include "user_flash.h"



// не выключать SWD отладчик в режиме сна!!!
//#define ENABLE_DEBUG_IN_LOWPOWER_MODE

/* -----------------------------------------------------------------------------
 * Константы
 * ---------------------------------------------------------------------------*/
#define MOTOR_PWM_MIN           84U
#define MOTOR_PWM_MAX           168U
#define MOTOR_PROTECT_TIME_MS   8000U
#define P_DERIVATIVE_LOW_THRESHOLD  5
#define IDLE_SLEEP_TIMEOUT_MS       10000U  /* Таймаут до перехода в Standby */

#define DISP_UPDATE_PERIOD_MS      500U
#define TYP_IDLE_VAC_VALUE      2048U
#define TYP_MAX_VAC_VALUE        920

/* -----------------------------------------------------------------------------
 * Перечисления и структуры
 * ---------------------------------------------------------------------------*/
typedef enum {
    MODE_IDLE,
    MODE_PUMP,
    MODE_TRACK,
    MODE_HOLD,
    MODE_WAKEUP
} OpMode_t;

static OpMode_t opMode = MODE_IDLE;

/* Таймеры: храним ?сколько осталось? в мс */
static struct {
    uint32_t toSleep;           /* до сна (только для IDLE) */
    uint32_t motorProtect;      /* защита мотора */
    uint32_t dispUpdate;        /* таймер обновления дисплея */
    uint32_t delay;             /* для блокирующей задержки */
    uint32_t task_500ms;        /* для задачи каждые 500 мс */
    uint32_t flag_500ms;
} timers;

/* Пороги вакуума */
static struct {
    uint16_t in_idle;
    uint16_t in_max;
    uint16_t thres60;
    uint16_t thres80;
} vac_values;

/* Глобальные переменные состояния */
static uint16_t motorPWM = 0U;
static uint16_t p_sensor_Filtered = 0U;
static int16_t  p_derivative = INT16_MAX;
static uint8_t  vac_percent = 0U;
static uint8_t  dispValue = 0U;
static int8_t maxVacCalibValue = 0;

/* -----------------------------------------------------------------------------
 * Прототипы
 * ---------------------------------------------------------------------------*/
static void Init_Vac_Values(uint16_t idle, uint16_t max);
static uint8_t Calc_Vac_percent(uint16_t value);
static void Vac_Start(void);
static void Vac_Stop(void);
static void Vac_Release(void);
static void Delay_ms(uint32_t ms);
static void Standby(void);  /* Объявление функции перехода в сон */
static void Task500(void);  /* каждые 500 мс выполняем */
static void SaveState(void);
static int8_t Clamp8(int16_t v);

/* -----------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/
int main(void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();

    /* Инициализация таймеров */
    timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;            /* В начале не спим */
    timers.motorProtect = 0U;
    timers.dispUpdate = DISP_UPDATE_PERIOD_MS;
    timers.task_500ms = 0U;

    
    
    uint8_t st;
    uint8_t resumeSleep = 0;
    if (Flash_Init() && Flash_Load(&st, &maxVacCalibValue)) {
        if (st == FL_ST_SLEEP) {
            resumeSleep = 1;
        } 
        else if (st == FL_ST_TRACK) {
            opMode = MODE_TRACK;
        }
        /* FL_ST_IDLE → opMode уже MODE_IDLE */

        /* инициализируем с сохраненной калибровкой */
        uint16_t maxVacValue = TYP_MAX_VAC_VALUE + maxVacCalibValue;
        Init_Vac_Values(TYP_IDLE_VAC_VALUE, maxVacValue);
    }
    else {
        // загружаем типовые значения
        Init_Vac_Values(TYP_IDLE_VAC_VALUE, TYP_MAX_VAC_VALUE);
    }



    Init();

    if (resumeSleep) {
        Standby();      /* сон здесь; после пробуждения — сразу в цикл, PIN_EN выключен */
    } 
    else {
        GPIO_SetBits(PIN_EN);
        Delay_ms(200U);
        Button_Enable();
    }

    while (1) {
        vac_percent = Calc_Vac_percent(p_sensor_Filtered);
        if (timers.flag_500ms) {
            Task500();
            timers.flag_500ms = 0;
        }

        SaveState();

        switch (opMode) {
            case MODE_IDLE: {
                /* Получаем событие кнопки (тип TBTNEVT определён в button.h) */
                TBTNEVT btnEvt = (TBTNEVT)Button_GetEvt();

                /* Если есть ЛЮБОЕ событие от кнопки — сбрасываем таймер сна */
                if (btnEvt != BTN_EVT_NONE) {
                    timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;
                }

                /* Обработка одиночного клика: старт накачки */
                if (btnEvt == BTN_EVT_CLICK) {
                    opMode = MODE_PUMP;
                    Vac_Start();
                    timers.motorProtect = MOTOR_PROTECT_TIME_MS;
                    break;
                }

                /* Логика перехода в сон по таймеру  либо по удержанию кнопки */
                if ((timers.toSleep == 0U) || 
                    (btnEvt == BTN_EVT_HOLD)) {
                    Standby();
                    break; 
                }
                break;
            }

            case MODE_PUMP: {
                /* Если откачка идет, перезагружаем таймер защиты */
                if (p_derivative > P_DERIVATIVE_LOW_THRESHOLD) {
                    timers.motorProtect = MOTOR_PROTECT_TIME_MS;
                }
                /* Если таймаут по защите или нажали кнопку - останов, сброс вакуума, выход в IDLE */
                if ((timers.motorProtect == 0U) || (Button_GetEvt() == BTN_EVT_CLICK)) {
                    Vac_Stop();
                    opMode = MODE_IDLE;
                    timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;
                    break;
                }

                if ((p_sensor_Filtered < vac_values.thres60) &&
                    (p_derivative < P_DERIVATIVE_LOW_THRESHOLD)) {
                    /* Откачка завершена — переходим в TRACK */
                    Init_Vac_Values(TYP_IDLE_VAC_VALUE, p_sensor_Filtered);
                    maxVacCalibValue = Clamp8((int16_t)p_sensor_Filtered - TYP_MAX_VAC_VALUE);
                    Vac_Stop();
                    opMode = MODE_TRACK;
                    break;
                }
                break;
            }

            case MODE_HOLD: {
                /* В HOLD: следим за порогом — если давление достаточно высокое, идём в TRACK */
                if (p_sensor_Filtered <= vac_values.thres80) {
                    Vac_Stop();
                    opMode = MODE_TRACK;
                    break;
                }
                /* Таймаут по защите - останов, сброс вакуума, выход в IDLE */
                if (timers.motorProtect == 0U) {
                    Vac_Stop();
                    Vac_Release();
                    opMode = MODE_IDLE;
                    timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;
                    break;
                }
                /* Кнопки в HOLD не обрабатываем (можно добавить, если нужно) */
                break;
            }

            case MODE_TRACK: {
                TBTNEVT btnEvt = (TBTNEVT)Button_GetEvt();

                /* В TRACK: одиночный игнорируем, двойной — сброс вакуума */
                if (btnEvt == BTN_EVT_DOUBLE) {
                    Vac_Release();
                    opMode = MODE_IDLE;
                    timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;
                    break;
                }

                /* Логика докачки: если упало ниже 60% — включаем мотор и идём в HOLD */
                if (p_sensor_Filtered > vac_values.thres60) {
                    Vac_Start();
                    timers.motorProtect = MOTOR_PROTECT_TIME_MS;
                    opMode = MODE_HOLD;
                    break;
                }
                break;
            }
            
            case MODE_WAKEUP:
                /* Ожидаем удержания кнопки 1 с */
                Delay_ms(50);   // пауза подавления дребезга
                timers.delay = 950;
                int btnHold = 1;
                while(timers.delay) {
                    if(GPIO_ReadInputDataBit(PIN_BTN)) {
                        btnHold = 0;
                        break;  /* while break */
                    }
                }
                if(!btnHold) {
                    /* кнопку отпустили раньше 1 с, засыпаем обратно */
                    Standby();
                    break;
                }
                else {
                    Init(); // реинициализация периферии и SysTick
                    GPIO_SetBits(PIN_EN);   // Вкл питания
                    timers.toSleep = IDLE_SLEEP_TIMEOUT_MS;
                    opMode = MODE_IDLE;
                    Button_Enable();
                }
            break;

            default:
                break;
        }
    }
}

/* -----------------------------------------------------------------------------
 * Прерывания
 * ---------------------------------------------------------------------------*/
void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick_Handler(void) {
    /* в режиме пробуждения работает только таймер для Delay_ms() */
    if(opMode == MODE_WAKEUP) {
        if (timers.delay > 0) {
            timers.delay--;
        }
        SysTick->SR = 0;
        return;
    }

    /* Стартуем преобразование */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    /* Плавный разгон мотора */
    if (motorPWM > 0U && motorPWM < MOTOR_PWM_MAX) {
        motorPWM++;
        TIM2->CH4CVR = motorPWM;
    } else if (motorPWM == 0U) {
        TIM2->CH4CVR = 0U;
    }

    Display_Refresh(dispValue);

    /* Декремент таймеров каждую миллисекунду */
    if (timers.delay > 0U) timers.delay--;
    if (timers.toSleep > 0U) timers.toSleep--;
    if (timers.motorProtect > 0U) timers.motorProtect--;
    if (timers.dispUpdate > 0U) timers.dispUpdate--;

    /* таймеры задач */
    if (timers.task_500ms > 0U ) {
        timers.task_500ms--;
    }
    else {
        timers.flag_500ms = 1;  // взводим флаг на исполнение
        timers.task_500ms = 500U;   // перезагружаем таймер задачи
    }

    /* Обновление отображаемого значения по таймеру */
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
 * Вспомогательные функции
 * ---------------------------------------------------------------------------*/
void Delay_ms(uint32_t ms) {
    timers.delay = ms;
    while(timers.delay) {
        ;
    }
}

static void Vac_Start(void) {
    /* Сначала старт мотора */
    motorPWM = MOTOR_PWM_MIN;
    TIM2->CH4CVR = motorPWM;
    TIM_CtrlPWMOutputs(TIM2, ENABLE);
    /* задержка */
    Delay_ms(200U);
    /* потом открываем клапан #1 */
    GPIO_SetBits(PIN_VAL1);
}

static void Vac_Stop(void) {
    /* Закрываем клапан #1 */
    GPIO_ResetBits(PIN_VAL1);
    Delay_ms(200U);
    /* Потом выкл. мотор */
    motorPWM = 0U;
    TIM2->CH4CVR = 0U;
    TIM_CtrlPWMOutputs(TIM2, DISABLE);
}

static void Vac_Release(void) {
    /* Открываем клапан #2 на 1 с */
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


static int8_t Clamp8(int16_t v) {
    if (v > 127)  return 127;
    if (v < -128) return -128;
    return (int8_t)v;
}


static void Task500(void) {
    // считаем производную давления dP/dt, она нужна в режиме PUMP!
    static uint32_t p_prev = 0;
    static int first = 1;

    if (first) {
        p_prev = p_sensor_Filtered;
        first = 0;
        p_derivative = INT16_MAX;  // оставляем твоё защитное значение
    } 
    else {
        int32_t diff = (int32_t)p_prev - (int32_t)p_sensor_Filtered;
        // Ограничиваем, чтобы не было переполнения при присваивании в int16_t
        if (diff > INT16_MAX) diff = INT16_MAX;
        if (diff < INT16_MIN) diff = INT16_MIN;
        p_derivative = (int16_t)diff;
        p_prev = p_sensor_Filtered;
    }
}


/* -----------------------------------------------------------------------------
 * Функция перехода в режим сна 
 * ---------------------------------------------------------------------------*/
void Standby(void) {

    Flash_Save(FL_ST_SLEEP, maxVacCalibValue);   /* помечаем ?спим? до отключения всего */

    Button_Disable();
    // Останавливаем SysTick, чтобы погасить дисплей
    NVIC_DisableIRQ(SysTick_IRQn);
    SysTick->CTLR &= ~STK_CTLR_STE;
    // Выкл питание и сенсор
    GPIO_ResetBits(PIN_EN);
    // Выкл дисплей
    GPIO_ResetBits(PIN_DISP_0);
    GPIO_ResetBits(PIN_DISP_1);

    // Выкл ОУ
    OPA_Cmd(DISABLE);
    
    // Выкл АЦП
    ADC_Cmd(ADC1, DISABLE);

    // Конфиг события для пробуждения
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    EXTI_InitTypeDef EXTI_InitStructure = {0};
    EXTI_InitStructure.EXTI_Line = EXTI_Line0;  // Кнопка на PD0
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    // Конфигурируем на GPIOD
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOD, GPIO_PinSource0);

    // на всякий очищаем старое событие
    EXTI_ClearITPendingBit(EXTI_Line0);
    // Усыпляем
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

#ifdef ENABLE_DEBUG_IN_LOWPOWER_MODE
#define PFIC_SCTLR_DEEPSLEEP    ((uint32_t)4)
/*!< Low-power mode of the control system. 1: DEEPSLEEP 0: SLEEP */
    /* Вход в Sleep mode вместо Standby, чтобы остался работать отладчик */
    PWR->CTLR = 0;
    NVIC->SCTLR &= ~PFIC_SCTLR_DEEPSLEEP;
    __WFE();
#else
    PWR_EnterSTANDBYMode(PWR_STANDBYEntry_WFE);
#endif

    /* После пробуждения: */
    Init_SysTick();
    opMode = MODE_WAKEUP;

}


static void SaveState(void) {
    switch (opMode) {
        case MODE_WAKEUP:
            break;       /* сон ещё не подтверждён — не пишем */

        case MODE_TRACK: /* Fall through */
        case MODE_HOLD:  
            Flash_Save(FL_ST_TRACK, maxVacCalibValue);
            break;

        default:
            Flash_Save(FL_ST_IDLE,  maxVacCalibValue);
            break;      /* PUMP и прочее → IDLE */
    }
}
