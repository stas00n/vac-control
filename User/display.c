#include "display.h"



/* §¢§Ú§ä§í §ã§Ö§Ô§Þ§Ö§ß§ä§à§Ó §ß§Ñ GPIOC[6:0] */
#define SEG_A   0x20    /* PC5 */
#define SEG_B   0x40    /* PC6 */
#define SEG_C   0x8     /* PC3 */
#define SEG_D   0x2     /* PC1 */
#define SEG_E   0x4     /* PC2 */
#define SEG_F   0x10    /* PC4 */
#define SEG_G   0x1     /* PC0 */
#define SEG_ALL 0x7f

static const uint8_t decodeTable[11] = {
    (SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F),    /* 0 */
    (SEG_B | SEG_C),                                    /* 1 */
    (SEG_A | SEG_B | SEG_D | SEG_E | SEG_G),            /* 2 */
    (SEG_A | SEG_B | SEG_C | SEG_D | SEG_G),            /* 3 */
    (SEG_B | SEG_C | SEG_F | SEG_G),                    /* 4 */
    (SEG_A | SEG_C | SEG_D | SEG_F | SEG_G),            /* 5 */ 
    (SEG_A | SEG_C | SEG_D | SEG_E |SEG_F | SEG_G),     /* 6 */
    (SEG_A | SEG_B | SEG_C ),                           /* 7 */
    (SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G),    /* 8 */
    (SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G),    /* 9 */
    (SEG_G)                                             /* - */
};


void Display_Refresh(uint8_t value) {
    static uint8_t dig = 0;     // §¶§Ñ§Ù§Ñ §Ú§ß§Õ§Ú§Ü§Ñ§è§Ú§Ú 0 1
    dig ^= 1;   // §±§Ö§â§Ö§Ü§Ý§ð§é§Ñ§Ö§Þ §æ§Ñ§Ù§å
    // §Ô§Ñ§ã§Ú§Þ §Ó§ã§Ö
    GPIO_ResetBits(GPIOC, SEG_ALL);
    GPIO_ResetBits(PIN_DISP_0);
    GPIO_ResetBits(PIN_DISP_1);

    if(value > 99) {
        // §à§ä§à§Ò§â§Ñ§Ø§Ñ§Ö§Þ -- 
        GPIO_SetBits(GPIOC, decodeTable[10]);
    }

    else {
        uint8_t dec; // §è§Ú§æ§â§Ñ
        if(dig) {
            // §ã§ä§Ñ§â§ê§Ú§Û §â§Ñ§Ù§â§ñ§Õ
            dec = value / 10;
        }
        else {
            // §Þ§Ý§Ñ§Õ§ê§Ú§Û §â§Ñ§Ù§â§ñ§Õ
            dec = value % 10;
        }
        GPIO_SetBits(GPIOC, decodeTable[dec]);
    }

    if(dig) {
       GPIO_SetBits(PIN_DISP_1);
    }
    else {
        GPIO_SetBits(PIN_DISP_0);
    }
}