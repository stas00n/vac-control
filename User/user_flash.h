#ifndef USER_FLASH_H
#define USER_FLASH_H

#include <stdint.h>

/* Классы состояния для энергонезависимого хранения */
#define FL_ST_IDLE    0x0
#define FL_ST_TRACK   0x1
#define FL_ST_SLEEP   0x2

/* 1 — найдены валидные сохранённые данные */
uint8_t Flash_Init(void);

/* 1 — данные есть; state и val заполнены */
uint8_t Flash_Load(uint8_t *state, int8_t *val);

/* Сохранить состояние. Дедупликация: при неизменных данных запись не выполняется. */
void    Flash_Save(uint8_t state, int8_t val);

#endif
