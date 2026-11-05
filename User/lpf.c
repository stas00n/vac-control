#include "lpf.h"


#define MY_LPF_SCALING_BITS 12
#define MY_LPF_A_NUM 125
#define MY_LPF_A_DENOM 128U
#define MY_LPF_B_NUM (MY_LPF_A_DENOM - MY_LPF_A_NUM)
#define MY_LPF_OUT_SCALING_BITS 2 /*!< выход расширяем до 12 бит */
/**
 * @brief Низкочастотный фильтр 1?го порядка в фиксированной точке.
 *
 * Реализует рекуррентную формулу:
 *   y[n] = a * y[n-1] + b * x[n],
 * где a = MY_LPF_A_NUM / MY_LPF_A_DENOM,
 *       b = MY_LPF_B_NUM / MY_LPF_A_DENOM.
 *
 * Все вычисления выполняются в масштабе MY_LPF_SCALING_BITS бит.
 * Деление на MY_LPF_A_DENOM (степень двойки) реализовано битовым сдвигом.
 *
 * Фильтр имеет внутреннее статическое состояние, инициализируется первым
 * вызовом значением входного отсчёта. Подходит для использования в одном месте.
 *
 * @param[in] x Сырой отсчёт АЦП (10 бит, диапазон 0–1023).
 *
 * @return Отфильтрованное значение в том же масштабе, что и вход (uint16_t).
 */
uint16_t LPF(uint16_t x) {
    static uint32_t lpf_initialized = 0;  // флаг инициализации
    static uint32_t y_scaled = 0;         // отфильтрованное значение в масштабе

    // Инициализация первым отсчётом
    if (lpf_initialized == 0) {
        y_scaled = (uint32_t)x << MY_LPF_SCALING_BITS;
        lpf_initialized = 1;
        return (x << MY_LPF_OUT_SCALING_BITS);
    }

    // y_scaled = a * y_scaled + b * (x << SCALING)
    y_scaled = y_scaled * MY_LPF_A_NUM
             + (uint32_t)MY_LPF_B_NUM * ((uint32_t)x << MY_LPF_SCALING_BITS);

    // Деление на 128 (2^7) через сдвиг
    y_scaled >>= 7U;

    return (uint16_t)(y_scaled >> (MY_LPF_SCALING_BITS - MY_LPF_OUT_SCALING_BITS));
}
