/* ========================================================================
 * [PROJECT]    SIR100
 * [MODULE]     Display
 * [TITLE]      display source file
 * [FILE]       display.c
 * [VSN]        1.0
 * [CREATED]    26092003
 * [LASTCHNGD]  06102006
 * [COPYRIGHT]  Copyright (C) STREAMIT BV
 * [PURPOSE]    contains all interface- and low-level routines to
 *              control the LCD and write characters or strings (menu-items)
 * ======================================================================== */

#define LOG_MODULE  LOG_DISPLAY_MODULE

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/timer.h>
#include <sys/event.h>
#include <sys/thread.h>
#include <sys/heap.h>

#include "system.h"
#include "portio.h"
#include "display.h"
#include "log.h"

/*-------------------------------------------------------------------------*/
/* local defines                                                           */
/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
/* local variable definitions                                              */
/*-------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------*/
/* local routines (prototyping)                                            */
/*-------------------------------------------------------------------------*/
static void LcdWriteByte(u_char, u_char);
static void LcdWriteNibble(u_char, u_char);
static void LcdWaitBusy(void);

/*!
 * \addtogroup Display
 */

/*@{*/

/*-------------------------------------------------------------------------*/
/*                         start of code                                   */
/*-------------------------------------------------------------------------*/

/* ����������������������������������������������������������������������� */
/*!
 * \brief control backlight
 */
/* ����������������������������������������������������������������������� */
void LcdBackLight(u_char Mode)
{
    if (Mode==LCD_BACKLIGHT_ON)
    {
        sbi(LCD_BL_PORT, LCD_BL_BIT);   // Turn on backlight
    }
    else if (Mode==LCD_BACKLIGHT_OFF)
    {
        cbi(LCD_BL_PORT, LCD_BL_BIT);   // Turn off backlight
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Write a single character on the LCD
 *
 * Writes a single character on the LCD on the current cursor position
 *
 * \param LcdChar character to write
 */
/* ����������������������������������������������������������������������� */
void LcdChar(char MyChar)
{
    LcdWriteByte(WRITE_DATA, MyChar);
}


/* ����������������������������������������������������������������������� */
/*!
 * \brief Low-level initialisation function of the LCD-controller
 *
 * Initialise the controller and send the User-Defined Characters to CG-RAM
 * settings: 4-bit interface, cursor invisible and NOT blinking
 *           1 line dislay, 10 dots high characters
 *
 */
/* ����������������������������������������������������������������������� */
 void LcdLowLevelInit()
{
    u_char i;

    NutDelay(140);                               // wait for more than 140 ms after Vdd rises to 2.7 V

    for (i=0; i<3; ++i)
    {
        LcdWriteNibble(WRITE_COMMAND, 0x33);      // function set: 8-bit mode; necessary to guarantee that
        NutDelay(4);                              // SIR starts up always in 5x10 dot mode
    }

    LcdWriteNibble(WRITE_COMMAND, 0x22);        // function set: 4-bit mode; necessary because KS0070 doesn't
    NutDelay(1);                                // accept combined 4-bit mode & 5x10 dot mode programming

    //LcdWriteByte(WRITE_COMMAND, 0x24);        // function set: 4-bit mode, 5x10 dot mode, 1-line
    LcdWriteByte(WRITE_COMMAND, 0x28);          // function set: 4-bit mode, 5x7 dot mode, 2-lines
    NutDelay(5);

    LcdWriteByte(WRITE_COMMAND, 0x0C);          // display ON/OFF: display ON, cursor OFF, blink OFF
    NutDelay(5);

    LcdWriteByte(WRITE_COMMAND, 0x01);          // display clear
    NutDelay(5);

    LcdWriteByte(WRITE_COMMAND, 0x06);          // entry mode set: increment mode, entire shift OFF


    LcdWriteByte(WRITE_COMMAND, 0x80);          // DD-RAM address counter (cursor pos) to '0'
}


/* ����������������������������������������������������������������������� */
/*!
 * \brief Low-level routine to write a byte to LCD-controller
 *
 * Writes one byte to the LCD-controller (by  calling LcdWriteNibble twice)
 * CtrlState determines if the byte is written to the instruction register
 * or to the data register.
 *
 * \param CtrlState destination: instruction or data
 * \param LcdByte byte to write
 *
 */
/* ����������������������������������������������������������������������� */
static void LcdWriteByte(u_char CtrlState, u_char LcdByte)
{
    LcdWaitBusy();                      // see if the controller is ready to receive next byte
    LcdWriteNibble(CtrlState, LcdByte & 0xF0);
    LcdWriteNibble(CtrlState, LcdByte << 4);

}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Low-level routine to write a nibble to LCD-controller
 *
 * Writes a nibble to the LCD-controller (interface is a 4-bit databus, so
 * only 4 databits can be send at once).
 * The nibble to write is in the upper 4 bits of LcdNibble
 *
 * \param CtrlState destination: instruction or data
 * \param LcdNibble nibble to write (upper 4 bits in this byte
 *
 */
/* ����������������������������������������������������������������������� */
static void LcdWriteNibble(u_char CtrlState, u_char LcdNibble)
{
    outp((inp(LCD_DATA_DDR) & 0x0F) | 0xF0, LCD_DATA_DDR);  // set data-port to output again

    outp((inp(LCD_DATA_PORT) & 0x0F) | (LcdNibble & 0xF0), LCD_DATA_PORT); // prepare databus with nibble to write

    if (CtrlState == WRITE_COMMAND)
    {
        cbi(LCD_RS_PORT, LCD_RS);     // command: RS low
    }
    else
    {
        sbi(LCD_RS_PORT, LCD_RS);     // data: RS high
    }

    sbi(LCD_EN_PORT, LCD_EN);

    asm("nop\n\tnop");                    // small delay

    cbi(LCD_EN_PORT, LCD_EN);
    cbi(LCD_RS_PORT, LCD_RS);
    outp((inp(LCD_DATA_DDR) & 0x0F), LCD_DATA_DDR);           // set upper 4-bits of data-port to input
    outp((inp(LCD_DATA_PORT) & 0x0F) | 0xF0, LCD_DATA_PORT);  // enable pull-ups in data-port
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Low-level routine to see if the controller is ready to receive
 *
 * This routine repeatedly reads the databus and checks if the highest bit (bit 7)
 * has become '0'. If a '0' is detected on bit 7 the function returns.
 *
 */
/* ����������������������������������������������������������������������� */
static void LcdWaitBusy()
{
    u_char Busy = 1;
	u_char LcdStatus = 0;

    cbi (LCD_RS_PORT, LCD_RS);              // select instruction register

    sbi (LCD_RW_PORT, LCD_RW);              // we are going to read

    while (Busy)
    {
        sbi (LCD_EN_PORT, LCD_EN);          // set 'enable' to catch 'Ready'

        asm("nop\n\tnop");                  // small delay
        LcdStatus =  inp(LCD_IN_PORT);      // LcdStatus is used elsewhere in this module as well
        Busy = LcdStatus & 0x80;            // break out of while-loop cause we are ready (b7='0')
    }

    cbi (LCD_EN_PORT, LCD_EN);              // all ctrlpins low
    cbi (LCD_RS_PORT, LCD_RS);
    cbi (LCD_RW_PORT, LCD_RW);              // we are going to write
}

/*-------------------------------------------------------------------------*/
/* Custom methods                                            */
/*-------------------------------------------------------------------------*/


/* ����������������������������������������������������������������������� */
/*!
 * \Writes bytes to specific locations.
 */
/* ����������������������������������������������������������������������� */
void WriteByteToLocation(u_char locationByte, u_char byteToWrite)
{
        LcdWriteByte(WRITE_COMMAND, locationByte);
        LcdWriteByte(WRITE_DATA, byteToWrite);
}

/* ����������������������������������������������������������������������� */
/*!
 * \Updates the time with the new time.
 */
/* ����������������������������������������������������������������������� */
void LcdTimeDisplay(char text[])
{
    int i;
    u_char c = '\0';
   
    if(strlen(text) > 8)
    {
        return;
    }
    
    for(i = 0; i < strlen(text); i++)
    {
        switch(i)
        {
            case 0:     c = LINE_1_9; break;
            case 1:     c = LINE_1_10; break;
            case 2:     c = LINE_1_11; break;
            case 3:     c = LINE_1_12; break;
            case 4:     c = LINE_1_13; break;
            case 5:     c = LINE_1_14; break;
            case 6:     c = LINE_1_15; break;
            case 7:     c = LINE_1_16; break;
        }
        WriteByteToLocation(c, text[i]);
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \Fills out the title portion of the screen.
 */
/* ����������������������������������������������������������������������� */
void LcdWriteTitle(char text[])
{
    int i;
    int maxChars = 7;
    u_char c = '\0';
    
    if(strlen(text) < 7)
    {
        maxChars = strlen(text);
    }
       
    for(i = 0; i < maxChars; i++)
    {
        switch(i)
        {
            case 0:     c = LINE_1_1; break;
            case 1:     c = LINE_1_2; break;
            case 2:     c = LINE_1_3; break;
            case 3:     c = LINE_1_4; break;
            case 4:     c = LINE_1_5; break;
            case 5:     c = LINE_1_6; break;
            case 6:     c = LINE_1_7; break;
        }
        WriteByteToLocation(c, text[i]);
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \Fills out the second row of the display.
 */
/* ����������������������������������������������������������������������� */
void LcdWriteSecondLine(char text[])
{
    int i;
    int maxChars = 16;
    u_char c = '\0';
    
    if(strlen(text) < 16)
    {
        maxChars = strlen(text);
    }
       
    for(i = 0; i < maxChars; i++)
    {
        switch(i)
        {
            case 0:     c = LINE_2_1; break;
            case 1:     c = LINE_2_2; break;
            case 2:     c = LINE_2_3; break;
            case 3:     c = LINE_2_4; break;
            case 4:     c = LINE_2_5; break;
            case 5:     c = LINE_2_6; break;
            case 6:     c = LINE_2_7; break;
            case 7:     c = LINE_2_8; break;
            case 8:     c = LINE_2_9; break;
            case 9:     c = LINE_2_10; break;
            case 10:     c = LINE_2_11; break;
            case 11:     c = LINE_2_12; break;
            case 12:     c = LINE_2_13; break;
            case 13:     c = LINE_2_14; break;
            case 14:     c = LINE_2_15; break;
            case 15:     c = LINE_2_16; break;
        }
        WriteByteToLocation(c, text[i]);
    }
}



/* ---------- end of module ------------------------------------------------ */

/*@}*/
