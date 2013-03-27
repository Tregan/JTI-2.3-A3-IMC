/* ========================================================================
 * [PROJECT]    SIR100
 * [MODULE]     Display
 * [TITLE]      display header file
 * [FILE]       display.h
 * [VSN]        1.0
 * [CREATED]    030414
 * [LASTCHNGD]  030414
 * [COPYRIGHT]  Copyright (C) STREAMIT BV 2010
 * [PURPOSE]    API and gobal defines for display module
 * ======================================================================== */

#ifndef _Display_H
#define _Display_H


/*-------------------------------------------------------------------------*/
/* global defines                                                          */
/*-------------------------------------------------------------------------*/
#define DISPLAY_SIZE                16
#define NROF_LINES                  2
#define MAX_SCREEN_CHARS            (NROF_LINES*DISPLAY_SIZE)

#define LINE_0                      0
#define LINE_1                      1

#define FIRSTPOS_LINE_0             0
#define FIRSTPOS_LINE_1             0x40


#define LCD_BACKLIGHT_ON            1
#define LCD_BACKLIGHT_OFF           0

#define ALL_ZERO          			0x00      // 0000 0000 B
#define WRITE_COMMAND     			0x02      // 0000 0010 B
#define WRITE_DATA        			0x03      // 0000 0011 B
#define READ_COMMAND      			0x04      // 0000 0100 B
#define READ_DATA         			0x06      // 0000 0110 B

/*--------------------------------------------------*/
/* custom defines                            		*/
/* \author Ricardo									*/
/*--------------------------------------------------*/

#define LINE_1_1                                0x80
#define LINE_1_2                                0x81
#define LINE_1_3                                0x82
#define LINE_1_4                                0x83
#define LINE_1_5                                0x84
#define LINE_1_6                                0x85
#define LINE_1_7                                0x86
#define LINE_1_8                                0x87
#define LINE_1_9                                0x88
#define LINE_1_10                               0x89
#define LINE_1_11                               0x8A
#define LINE_1_12                               0x8B
#define LINE_1_13                               0x8C
#define LINE_1_14                               0x8D
#define LINE_1_15                               0x8E
#define LINE_1_16                               0x8F

#define LINE_2_1                                0xC0
#define LINE_2_2                                0xC1
#define LINE_2_3                                0xC2
#define LINE_2_4                                0xC3
#define LINE_2_5                                0xC4
#define LINE_2_6                                0xC5
#define LINE_2_7                                0xC6
#define LINE_2_8                                0xC7
#define LINE_2_9                                0xC8
#define LINE_2_10                               0xC9
#define LINE_2_11                               0xCA
#define LINE_2_12                               0xCB
#define LINE_2_13                               0xCC
#define LINE_2_14                               0xCD
#define LINE_2_15                               0xCE
#define LINE_2_16                               0xCF

/*-------------------------------------------------------------------------*/
/* typedefs & structs                                                      */
/*-------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*  Global variables                                                        */
/*--------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/* export global routines (interface)                                      */
/*-------------------------------------------------------------------------*/
extern void LcdChar(char);
extern void LcdBackLight(u_char);
extern void LcdInit(void);
extern void LcdLowLevelInit(void);

/*----------------------------------------------------------*/
/* export custom global routines                            */
/* \author Ricardo				                            */
/*----------------------------------------------------------*/
void LcdTimeDisplay(char text[]);
void LcdWriteTitle(char text[]);
void LcdWriteFirstLine(char text[]);
void LcdWriteSecondLine(char text[]);

void LcdClearAll(void);
void LcdClearTitle(void);
void LcdClearFirstLine(void);
void LcdClearSecondLine(void);
void LcdClearTitleLine(void);

void LcdClearLine(void); //Deprecated

#endif /* _Display_H */
/*  ����  End Of File  �������� �������������������������������������������� */





