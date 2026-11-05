#ifndef USER_FLASH_H
#define USER_FLASH_H

#include <stdint.h>

/* §¬§Ý§Ñ§ã§ã§í §ã§à§ã§ä§à§ñ§ß§Ú§ñ §Õ§Ý§ñ §ï§ß§Ö§â§Ô§à§ß§Ö§Ù§Ñ§Ó§Ú§ã§Ú§Þ§à§Ô§à §ç§â§Ñ§ß§Ö§ß§Ú§ñ */
#define FL_ST_IDLE    0x0
#define FL_ST_TRACK   0x1
#define FL_ST_SLEEP   0x2

/* 1 ¡ª §ß§Ñ§Û§Õ§Ö§ß§í §Ó§Ñ§Ý§Ú§Õ§ß§í§Ö §ã§à§ç§â§Ñ§ß§×§ß§ß§í§Ö §Õ§Ñ§ß§ß§í§Ö */
uint8_t Flash_Init(void);

/* 1 ¡ª §Õ§Ñ§ß§ß§í§Ö §Ö§ã§ä§î; state §Ú val §Ù§Ñ§á§à§Ý§ß§Ö§ß§í */
uint8_t Flash_Load(uint8_t *state, int8_t *val);

/* §³§à§ç§â§Ñ§ß§Ú§ä§î §ã§à§ã§ä§à§ñ§ß§Ú§Ö. §¥§Ö§Õ§å§á§Ý§Ú§Ü§Ñ§è§Ú§ñ: §á§â§Ú §ß§Ö§Ú§Ù§Þ§Ö§ß§ß§í§ç §Õ§Ñ§ß§ß§í§ç §Ù§Ñ§á§Ú§ã§î §ß§Ö §Ó§í§á§à§Ý§ß§ñ§Ö§ä§ã§ñ. */
void    Flash_Save(uint8_t state, int8_t val);

#endif
