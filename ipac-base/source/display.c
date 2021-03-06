/* ========================================================================
 * [PROJECT]    SIR100
 * [MODULE]     Display
 * [TITLE]      display source file
 * [FILE]       display.c
 * [VSN]        1.0
 * [CREATED]    26092003
 * [LASTCHNGD]  27032013
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

char * title = NULL;
char * secondline = NULL;
int titlebool = 0;
int secondlinebool = 0;

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

/* ����������������������������������������������������������������������� */
/*!
 * \brief Writes bytes to specific locations.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void WriteByteToLocation(u_char locationByte, u_char byteToWrite)
{
        LcdWriteByte(WRITE_COMMAND, locationByte);
        LcdWriteByte(WRITE_DATA, byteToWrite);
}

/*-------------------------------------------------------------------------*/
/* Custom methods                                            */
/*-------------------------------------------------------------------------*/

/* ����������������������������������������������������������������������� */
/*!
 * \brief Writes the title on the display.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void LcdWriteShortTitle(char text[])
{
    int i = 1;
    
    WriteByteToLocation(LINE_1_1, text[0]);
    for(i = 1; i < 7; i++)
    {
        if(text[i] == '\0')
        {
            return;
        }
        
        LcdChar(text[i]);
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Writes the first line on the display.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void LcdWriteShortFirstLine(char text[])
{
    int i;
    
    WriteByteToLocation(LINE_1_1, text[0]);
    for(i = 1; i < 16; i++)
    {
        if(text[i] == '\0')
        {
            return;
        }
        
        LcdChar(text[i]);
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Writes the second line on the display.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void LcdWriteShortSecondLine(char text[])
{
    int i;
    
    WriteByteToLocation(LINE_2_1, text[0]);
    for(i = 1; i < 16; i++)
    {
        if(text[i] == '\0')
        {
            return;
        }
        
        LcdChar(text[i]);
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Updates the time with the new time.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void LcdTimeDisplay(char text[])
{
    int i;
    
    WriteByteToLocation(LINE_1_9, text[0]);
    for(i = 1; i < strlen(text); i++)
    {
        LcdChar(text[i]);
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Clears the title.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void LcdClearTitle()
{
        LcdWriteTitle("       ");
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Clears the first line.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void LcdClearFirstLine()
{
        LcdWriteFirstLine("                ");
        LcdClearTitle();
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Clears the second line.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void LcdClearSecondLine()
{
        LcdWriteSecondLine("                ");
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Clears the second line.
 * \author Ricardo Blommers.
 * \deprecated
 */
/* ����������������������������������������������������������������������� */
void LcdClearLine()
{
        LcdClearSecondLine();
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Clears both lines.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void LcdClearTitleLine()
{
    LcdWriteShortTitle("        ");
    LcdClearSecondLine();
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Clears the display of all text.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void LcdClearAll()
{
        LcdClearFirstLine();
        LcdClearSecondLine();
}

/*-------------------------------------------------------------------------*/
/*                         Threads                                 */
/*-------------------------------------------------------------------------*/

/* ����������������������������������������������������������������������� */
/*!
 * \brief Thread to make the title scroll.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
THREAD(TitleThread, args)
{
    int i = 0;
    int count = 0;
    char shorterString[7];
        
    for(;;)
    {   
        if(titlebool == 1)
        {
            titlebool = 2;
            count = 0;
        }
        
        if(strlen(title) < 7)
        {
            LcdWriteShortTitle("       ");
            LcdWriteShortTitle(title);
        }
        else
        {
            if(count == 1)
            {
                NutSleep(400);
            }

            memset(shorterString, 0, 7);
            for(i = count; i < count + 7; i++)
            {
                shorterString[i - count] = title[i];
            }

            LcdWriteShortTitle(shorterString);
            count++;
            
            if(title[count + 6] == '\0')
            {
                //LogMsg_P(LOG_INFO, PSTR("End of title detected"));
                count = 0;
                NutSleep(1000);
            }
            NutSleep(650);
        }
        NutSleep(100);
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Thread to scroll the second line.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
THREAD(SecondLineThread, args)
{
    int i = 0;
    int count = 0;
    char shorterString[16];
        
    for(;;)
    {        
        if(secondlinebool == 1)
        {
            secondlinebool = 2;
            count = 0;
        }
        
        if(strlen(secondline) < 16)
        {
            LcdWriteShortSecondLine("                ");
            LcdWriteShortSecondLine(secondline);
        }
        else
        {
            if(count == 1)
            {
                NutSleep(400);
            }

            memset(shorterString, 0, 16);
            for(i = count; i < count + 16; i++)
            {
                shorterString[i - count] = secondline[i];
            }

            LcdWriteShortSecondLine(shorterString);
            count++;
            
            if(secondline[count + 15] == '\0')
            {
                //LogMsg_P(LOG_INFO, PSTR("End of second line detected"));
                count = 0;
                NutSleep(1000);
            }
            NutSleep(650);
        }
        NutSleep(100);
    }
}

/*-------------------------------------------------------------------------*/
/*                         Methods from which threads are called.          */
/*-------------------------------------------------------------------------*/


/* ����������������������������������������������������������������������� */
/*!
 * \brief Fills out the title portion of the screen.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void LcdWriteTitle(char text[])
{
    title = text;
    if(titlebool == 0)
    {
        NutThreadSetPriority(1);
        NutThreadCreate("LcdA", TitleThread, text, 1024);
        NutThreadSetPriority(0);
    }
    titlebool = 1;
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Fills out the first row of the display.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void LcdWriteFirstLine(char text[])
{    
    LcdWriteShortFirstLine(text);
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Fills out the second row of the display.
 * \author Ricardo Blommers.
 */
/* ����������������������������������������������������������������������� */
void LcdWriteSecondLine(char text[])
{    
    secondline = text;
    if(secondlinebool == 0)
    {
        NutThreadSetPriority(1);
        NutThreadCreate("LcdB", SecondLineThread, text, 1024);
        NutThreadSetPriority(0);
    }
    secondlinebool = 1;
}

/* ---------- end of module ------------------------------------------------ */

/*@}*/
