/********************************** (C) COPYRIGHT  *******************************
* File Name          : SysTick.h
* Author             : stas00n
* Version            : V1.0.1
* Date               : 2026/10/1
* Description        : RISC-V V2 SysTick Bit definitions
*********************************************************************************/
#ifndef _SYSTICK_H_
#define _SYSTICK_H_

/** System counter enable control bit. */
#define STK_CTLR_STE        0x1
#define STK_CTLR_STIE       0x2
#define STK_CTLR_STCLK      0x4
#define STK_CTLR_STRE       0x8
#define STK_CTLR_SWIE       0x80000000

/** Counting value comparison flag, write 0 clear, write 1 invalid */
#define STK_SR_CNTIF        1


#endif /* _SYSTICK_H_ */
