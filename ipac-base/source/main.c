/*! \mainpage SIR firmware documentation
 *
 *  \section intro Introduction
 *  A collection of HTML-files has been generated using the documentation in the sourcefiles to
 *  allow the developer to browse through the technical documentation of this project.
 *  \par
 *  \note these HTML files are automatically generated (using DoxyGen) and all modifications in the
 *  documentation should be done via the sourcefiles.
 */

/*! \file
 *  COPYRIGHT (C) STREAMIT BV 2010
 *  \date 19 december 2003
 */
 
#define LOG_MODULE  LOG_MAIN_MODULE

/*--------------------------------------------------------------------------*/
/*  Include files                                                           */
/*--------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <sys/thread.h>
#include <sys/timer.h>
#include <sys/version.h>
#include <dev/irqreg.h>
#include <time.h>

#include "system.h"
#include "portio.h"
#include "display.h"
#include "remcon.h"
#include "keyboard.h"
#include "led.h"
#include "log.h"
#include "uart0driver.h"
#include "mmc.h"
#include "watchdog.h"
#include "flash.h"
#include "spidrv.h"
#include "network.h"
#include "rtc.h"
#include "menu.h"
#include "main.h"

/*-------------------------------------------------------------------------*/
/* global variable definitions                                             */
/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/* local variable definitions                                              */
/*-------------------------------------------------------------------------*/
//Enable or disable debugging messages and functionality (1 = on, 0 = off)
const int DEBUG = 0;

//General
tm datetime;
int firstStartup;
int threadExit;
//backlightCounter
int backlightCounter;
int backlightStayOn;
//Timezone
int timezone;
int newTimezone;
int timeZoneSet;
int setTimezoneFromMenu;
//Time and Date
int selectedDatetimeUnit;
int pauseCurrentDatetime;
int datetimeSetManually;

/*-------------------------------------------------------------------------*/
/* local routines (prototyping)                                            */
/*-------------------------------------------------------------------------*/
static void SysMainBeatInterrupt(void*);
static void SysControlMainBeat(u_char);

/*-------------------------------------------------------------------------*/
/* Stack check variables placed in .noinit section                         */
/*-------------------------------------------------------------------------*/

/*!
 * \addtogroup System
 */

/*@{*/


/*-------------------------------------------------------------------------*/
/*                         start of code                                   */
/*-------------------------------------------------------------------------*/


/* ����������������������������������������������������������������������� */
/*!
 * \brief ISR MainBeat Timer Interrupt (Timer 2 for Mega128, Timer 0 for Mega256).
 *
 * This routine is automatically called during system
 * initialization.
 *
 * resolution of this Timer ISR is 4,448 msecs
 *
 * \param *p not used (might be used to pass parms from the ISR)
 */
/* ����������������������������������������������������������������������� */
static void SysMainBeatInterrupt(void *p)
{
    /*
     *  scan for valid keys AND check if a MMCard is inserted or removed
     */
    KbScan();
    CardCheckCard();
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Initialise Digital IO
 *  init inputs to '0', outputs to '1' (DDRxn='0' or '1')
 *
 *  Pull-ups are enabled when the pin is set to input (DDRxn='0') and then a '1'
 *  is written to the pin (PORTxn='1')
 */
/* ����������������������������������������������������������������������� */
void SysInitIO(void)
{
    /*
     *  Port B:     VS1011, MMC CS/WP, SPI
     *  output:     all, except b3 (SPI Master In)
     *  input:      SPI Master In
     *  pull-up:    none
     */
    outp(0xF7, DDRB);

    /*
     *  Port C:     Address bus
     */

    /*
     *  Port D:     LCD_data, Keypad Col 2 & Col 3, SDA & SCL (TWI)
     *  output:     Keyboard colums 2 & 3
     *  input:      LCD_data, SDA, SCL (TWI)
     *  pull-up:    LCD_data, SDA & SCL
     */
    outp(0x0C, DDRD);
    outp((inp(PORTD) & 0x0C) | 0xF3, PORTD);

    /*
     *  Port E:     CS Flash, VS1011 (DREQ), RTL8019, LCD BL/Enable, IR, USB Rx/Tx
     *  output:     CS Flash, LCD BL/Enable, USB Tx
     *  input:      VS1011 (DREQ), RTL8019, IR
     *  pull-up:    USB Rx
     */
    outp(0x8E, DDRE);
    outp((inp(PORTE) & 0x8E) | 0x01, PORTE);

    /*
     *  Port F:     Keyboard_Rows, JTAG-connector, LED, LCD RS/RW, MCC-detect
     *  output:     LCD RS/RW, LED
     *  input:      Keyboard_Rows, MCC-detect
     *  pull-up:    Keyboard_Rows, MCC-detect
     *  note:       Key row 0 & 1 are shared with JTAG TCK/TMS. Cannot be used concurrent
     */
#ifndef USE_JTAG
    sbi(JTAG_REG, JTD); // disable JTAG interface to be able to use all key-rows
    sbi(JTAG_REG, JTD); // do it 2 times - according to requirements ATMEGA128 datasheet: see page 256
#endif //USE_JTAG

    outp(0x0E, DDRF);
    outp((inp(PORTF) & 0x0E) | 0xF1, PORTF);

    /*
     *  Port G:     Keyboard_cols, Bus_control
     *  output:     Keyboard_cols
     *  input:      Bus Control (internal control)
     *  pull-up:    none
     */
    outp(0x18, DDRG);
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Starts or stops the 4.44 msec mainbeat of the system
 * \param OnOff indicates if the mainbeat needs to start or to stop
 */
/* ����������������������������������������������������������������������� */
static void SysControlMainBeat(u_char OnOff)
{
    int nError = 0;

    if (OnOff==ON)
    {
        nError = NutRegisterIrqHandler(&OVERFLOW_SIGNAL, SysMainBeatInterrupt, NULL);
        if (nError == 0)
        {
            init_8_bit_timer();
        }
    }
    else
    {
        // disable overflow interrupt
        disable_8_bit_timer_ovfl_int();
    }
}

/*-------------------------------------------------------------------------*/
/* Threads                                                                 */
/*-------------------------------------------------------------------------*/

/* ����������������������������������������������������������������������� */
/*!
 * \brief Thread for setting the timeZone
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
THREAD(KBThreadTimeZone, args)
{
    for(;;)
    {
        NutSleep(200);
        //Wait for keyboard event
        if(KbWaitForKeyEvent(500) != KB_ERROR)
        {
            u_char key = KbGetKey();
            if(key == KEY_UP)
                if(newTimezone <= 13)
                    newTimezone++;
                else
                    newTimezone = -12;
            else if(key == KEY_DOWN)
                if(newTimezone >= -11)
                    newTimezone--;
                else
                    newTimezone = 14;
            else if(key == KEY_OK)
            {
                timeZoneSet = 1;
                //Thread no longer needed, exit please
                NutThreadExit();
            }
            else if(key == KEY_ESC  && !firstStartup)
            {
                threadExit = 1;
                //Thread no longer needed, exit please
                NutThreadExit();
            }
        }
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Thread for setting the time manually
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
THREAD(KBThreadManualTime, args)
{
    //Time units: 0 = hours, 1 = minutes, 2 = seconds
    selectedDatetimeUnit = DATETIME_HOURS;
    
    for(;;)
    {
        NutSleep(300);
        //Wait for keyboard event
        if(KbWaitForKeyEvent(500) != KB_ERROR)
        {
            u_char key = KbGetKey();
            if(key == KEY_UP)
            {
                switch(selectedDatetimeUnit)
                {
                    case DATETIME_HOURS:
                        if(datetime.tm_hour >= 23)
                            datetime.tm_hour = 0;
                        else
                            datetime.tm_hour++;
                        break;
                    case DATETIME_MINUTES:
                        if(datetime.tm_min >= 59)
                            datetime.tm_min = 0;
                        else
                            datetime.tm_min++;
                        break;
                    case DATETIME_SECONDS:
                        if(datetime.tm_sec >= 59)
                            datetime.tm_sec = 0;
                        else
                            datetime.tm_sec++;
                        break;
                    default:
                        break;
                }
            }
            else if(key == KEY_DOWN)
            {
                switch(selectedDatetimeUnit)
                {
                    case DATETIME_HOURS:
                        if(datetime.tm_hour <= 0)
                            datetime.tm_hour = 23;
                        else
                            datetime.tm_hour--;
                        break;
                    case DATETIME_MINUTES:
                        if(datetime.tm_min <= 0)
                            datetime.tm_min = 59;
                        else
                            datetime.tm_min--;
                        break;
                    case DATETIME_SECONDS:
                        if(datetime.tm_sec <= 0)
                            datetime.tm_sec = 59;
                        else
                            datetime.tm_sec--;
                        break;
                    default:
                        break;
                }
            }
            else if(key == KEY_LEFT)
            {
                //Set to seconds if it already is at hours
                if(selectedDatetimeUnit <= DATETIME_HOURS)
                {
                    selectedDatetimeUnit = DATETIME_SECONDS;
                }
                else
                    selectedDatetimeUnit--;
            }
            else if(key == KEY_RIGHT)
            {
                //Set to hours if it already is at seconds
                if(selectedDatetimeUnit >= DATETIME_SECONDS)
                {
                    selectedDatetimeUnit = DATETIME_HOURS;
                }
                else
                    selectedDatetimeUnit++;
            }
            else if(key == KEY_OK)
            {
                datetimeSetManually = 1;
                //Thread no longer needed, exit please
                NutThreadExit();
            }
            else if(key == KEY_ESC && !firstStartup)
            {
                threadExit = 1;
                //Thread no longer needed, exit please
                NutThreadExit();
            }
        }
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Thread for setting the date manually
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
THREAD(KBThreadManualDate, args)
{
    //Time units: 0 = days, 1 = months, 2 = years
    selectedDatetimeUnit = DATETIME_DAYS;
    
    for(;;)
    {
        NutSleep(300);
        //Wait for keyboard event
        if(KbWaitForKeyEvent(500) != KB_ERROR)
        {
            u_char key = KbGetKey();
            if(key == KEY_UP)
            {
                switch(selectedDatetimeUnit)
                {
                    case DATETIME_DAYS:
                        if(datetime.tm_mday >= 30)
                            datetime.tm_mday = 1;
                        else
                            datetime.tm_mday++;
                        break;
                    case DATETIME_MONTHS:
                        if(datetime.tm_mon >= 11)
                            datetime.tm_mon = 0;
                        else
                            datetime.tm_mon++;
                        break;
                    case DATETIME_YEARS:
                            datetime.tm_year++;
                        break;
                    default:
                        break;
                }
            }
            else if(key == KEY_DOWN)
            {
                switch(selectedDatetimeUnit)
                {
                    case DATETIME_DAYS:
                        if(datetime.tm_mday <= 1)
                            datetime.tm_mday = 31;
                        else
                            datetime.tm_mday--;
                        break;
                    case DATETIME_MONTHS:
                        if(datetime.tm_mon <= 0)
                            datetime.tm_mon = 11;
                        else
                            datetime.tm_mon--;
                        break;
                    case DATETIME_YEARS:
                        if(datetime.tm_year > 0)
                            datetime.tm_year--;
                        break;
                    default:
                        break;
                }
            }
            else if(key == KEY_LEFT)
            {
                //Set to seconds if it already is at days
                if(selectedDatetimeUnit <= DATETIME_DAYS)
                {
                    selectedDatetimeUnit = DATETIME_YEARS;
                }
                else
                    selectedDatetimeUnit--;
            }
            else if(key == KEY_RIGHT)
            {
                //Set to hours if it already is at seconds
                if(selectedDatetimeUnit >= DATETIME_YEARS)
                {
                    selectedDatetimeUnit = DATETIME_DAYS;
                }
                else
                    selectedDatetimeUnit++;
            }
            else if(key == KEY_OK)
            {
                datetimeSetManually = 1;
                //Thread no longer needed, exit please
                NutThreadExit();
            }
            else if(key == KEY_ESC  && !firstStartup)
            {
                threadExit = 1;
                //Thread no longer needed, exit please
                NutThreadExit();
            }
        }
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Thread for setting the backlight
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
THREAD(BacklightThread, args)
{
    backlightCounter = 0;
    for(;;)
    {
        NutSleep(200);
        u_char key = KbGetKey();
        if(key != KEY_UNDEFINED)
        {
            backlightCounter = 0;
            LcdBackLight(LCD_BACKLIGHT_ON);
        }
        
        if(!backlightStayOn)
            backlightCounter++;
        
        if(backlightCounter >= 25)
            LcdBackLight(LCD_BACKLIGHT_OFF);
    }
}

/*-------------------------------------------------------------------------*/
/* Functions                                                                */
/*-------------------------------------------------------------------------*/

/* ����������������������������������������������������������������������� */
/*!
 * \brief Set _timezone of time.h
 * \author Niels & Bas
 */
/* ����������������������������������������������������������������������� */
void SetTimezone_timeh(int* timezone)
{
    if((-12 <= *timezone) && (*timezone <= 14))
    {
        printf("\ntimezone = %d", *timezone);
        _timezone = -*timezone * 60 * 60;
    }
    else
    {
        _timezone = 0;
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief reset the backlight counter
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
void ResetBacklightCounter(void)
{
    backlightCounter = 0;
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief set backlight to stay on or not, no matter the backlightCounter
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
void SetBacklightStayOn(int stayOn)
{
    backlightStayOn = stayOn;
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Displays the current time (from RTC) on the screen
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
void ShowCurrentTime(void)
{
    //Display time
    if (X12RtcGetClock(&datetime) == 0)
    {
        if(DEBUG)
            LogMsg_P(LOG_INFO, PSTR("RTC time [%02d:%02d:%02d]"), datetime.tm_hour, datetime.tm_min, datetime.tm_sec);
        
        //Create an output for the string
        char output[20];
        //Create string from the time ints
        sprintf(output, "%02d:%02d:%02d", datetime.tm_hour, datetime.tm_min, datetime.tm_sec);
        //Display the current time
        LcdTimeDisplay(output);
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Try to syns the time and date with an NTP server
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
void SyncDatetime(void)
{
    //Query NTP server and set time. If no time was set, set it manually.
    if(!NTP(&datetime))
    {
        LcdWriteFirstLine("Failed.");
        LcdWriteSecondLine("No time obtained");
        NutSleep(2000);
        SetTimeManually();
        SetDateManually();
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Starts the check for firstStartup, and if so: waits for timeZone input
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
void SetTimezone(void)
{
    //Clear display
    LcdClearAll();

    At45dbPageRead(0 + sizeof(int), &timezone, sizeof(int));
    
    if(DEBUG)
	LogMsg_P(LOG_INFO, PSTR("Value of timeZone: %d"), timezone);
    
    if(timezone < -12 || timezone > 14)
        timezone = 0;
    
    //First startup or called from menu, set the timezone!
    if(firstStartup || setTimezoneFromMenu)
    {
        newTimezone = timezone;
        
        if(DEBUG)
            LogMsg_P(LOG_INFO, PSTR("Waiting for timeZone input"));

        //Backlight should stay on during setting
        SetBacklightStayOn(BACKLIGHT_ON);
        if(firstStartup)
            LcdWriteFirstLine("Set Timezone");
        else
            LcdWriteTitle("Set Timezone");
        timeZoneSet = 0;
        
        //Create the Keyboard Thread for setting the timeZone
        NutThreadCreate("KBThreadTimeZone", KBThreadTimeZone, NULL, 1024);

        while(timeZoneSet != 1)
        {
            NutSleep(100);
            
            //If the user wants to quit, return
            if(threadExit)
            {
                //Clear display
                LcdClearAll();

                threadExit = 0;
                return;
            }

            //array of 20 chars should be enough for "UTC +(or -)14"
            char output[20];
            if(newTimezone > -1)
                sprintf(output, "UTC +%d ", newTimezone);
            else
                sprintf(output, "UTC %d ", newTimezone);
            LcdWriteSecondLine(output);

            WatchDogRestart();
        }

        //Backlight can go out again
        SetBacklightStayOn(BACKLIGHT_OFF);

        //Time changed, because timezone changed
        if(setTimezoneFromMenu)
        {
            int timezoneDifference = newTimezone - timezone;
            
            datetime.tm_hour += timezoneDifference;
            //Hours can ofcourse never be more than 23...
            if(datetime.tm_hour > 23)
                datetime.tm_hour -= 24;
            //...Or less than 0
            else if(datetime.tm_hour < 0)
                datetime.tm_hour = 24 + datetime.tm_hour;
            //Set the timezone to the newly set timezone
            timezone = newTimezone;
            
            //Write the gmt struct to RTC
            X12RtcSetClock(&datetime);
        }
        
        At45dbPageWrite(0 + sizeof(int), &timezone, sizeof(int));
    }
    
    LogMsg_P(LOG_INFO, PSTR("Value of timeZone: %d"), timezone);
        
    //Clear the display
    LcdClearAll();

    //Set timezone of time.h to our timezone
    SetTimezone_timeh(&timezone);
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Manually set the time
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
void SetTimeManually(void)
{
    //Create a backup of the current datetime
    tm datetimeBackup = datetime;
    
    //While setting the time, we do not want to see it updated, undoing our changes
    pauseCurrentDatetime = 1;
    
    //Get the current time from RTC and store it in gmt struct
    X12RtcGetClock(&datetime);
            
    datetimeSetManually = 0;
    
    //Create the Keyboard Thread for setting the timeZone
    NutThreadCreate("KBThreadManualTime", KBThreadManualTime, NULL, 1024);
    
    //Backlight should stay on during setting
    SetBacklightStayOn(BACKLIGHT_ON);
    //Clear the title
    LcdClearAll();
    if(firstStartup)
        LcdWriteFirstLine("Set Manual Time");
    else
    {
        LcdWriteTitle("Set Manual Time");
        //Show the current time, but do NOT update it! We're setting a time, updating while setting would be very silly
        ShowCurrentTime();
    }
    
    char output[20];
    
    while(datetimeSetManually != 1)
    {
        NutSleep(100);
        
        //If the user wants to quit, return
        if(threadExit)
        {
            //Clear display
            LcdClearAll();
            
            threadExit = 0;
            //Done, start updating the current time again
            pauseCurrentDatetime = 0;
            return;
        }
        
        //Clear the second line
        LcdClearLine();
        
        switch(selectedDatetimeUnit)
        {
            case 0:
                sprintf(output, "Set Hours: %02d", datetime.tm_hour);
                LcdWriteSecondLine(output);
                break;
            case 1:
                sprintf(output, "Set Minutes: %02d", datetime.tm_min);
                LcdWriteSecondLine(output);
                break;
            case 2:
                sprintf(output, "Set Seconds: %02d", datetime.tm_sec);
                LcdWriteSecondLine(output);
                break;
            default:
                break;
        }
    }
    //Clear display
    LcdClearAll();
    //Backlight no longer needed to stay on
    SetBacklightStayOn(BACKLIGHT_OFF);
    //Write the datetime struct to RTC. In case of an error, set the backup to datetime
    if(X12RtcSetClock(&datetime) != 0)
        datetime = datetimeBackup;
    //Done, start updating the current time again
    pauseCurrentDatetime = 0;
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Manually set the time
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
void SetDateManually(void)
{
    //Create a backup of the current datetime
    tm datetimeBackup = datetime;
    
    //While setting the time, we do not want to see it updated, undoing our changes
    pauseCurrentDatetime = 1;
    
    //Get the current time from RTC and store it in gmt struct
    X12RtcGetClock(&datetime);
            
    datetimeSetManually = 0;
    
    //Create the Keyboard Thread for setting the timeZone
    NutThreadCreate("KBThreadManualTime", KBThreadManualDate, NULL, 1024);
    
    //Backlight should stay on during setting
    SetBacklightStayOn(BACKLIGHT_ON);
    //Clear the title
    LcdClearAll();
    if(firstStartup)
        LcdWriteFirstLine("Set Manual Date");
    else
        LcdWriteTitle("Set Manual Date");
    
    //TODO, if time left. Not really important and needs the displaying of time changed...
    //Show the current time, but do NOT update it! We're setting a time, updating while setting would be very silly
    //showCurrentDate();
    
    char output[20];
    
    while(datetimeSetManually != 1)
    {
        NutSleep(100);
        
        //If the user wants to quit, return
        if(threadExit)
        {
            //Clear display
            LcdClearAll();
            
            threadExit = 0;
            //Done, start updating the current time again
            pauseCurrentDatetime = 0;
            return;
        }
        
        //Clear the second line
        LcdClearLine();
        
        switch(selectedDatetimeUnit)
        {
            case 0:
                sprintf(output, "Set Days: %02d", datetime.tm_mday);
                LcdWriteSecondLine(output);
                break;
            case 1:
                sprintf(output, "Set Months: %02d", datetime.tm_mon + 1);
                LcdWriteSecondLine(output);
                break;
            case 2:
                sprintf(output, "Set Years: %02d", datetime.tm_year + 1900);
                LcdWriteSecondLine(output);
                break;
            default:
                break;
        }
    }
    //Clear display
    LcdClearAll();
    //Backlight no longer needed to stay on
    SetBacklightStayOn(BACKLIGHT_OFF);
    //Write the datetime struct to RTC. In case of an error, set the backup to datetime
    //TODO if there's time left, make a calendar that checks if the date is valid
    if(X12RtcSetClock(&datetime) != 0)
        datetime = datetimeBackup;
    
    printf("\nSuccess. NTP time is: %02d:%02d:%02d %d-%d-%d", datetime.tm_hour, datetime.tm_min, datetime.tm_sec, datetime.tm_mday, datetime.tm_mon + 1, datetime.tm_year + 1900);

    //Done, start updating the current time again
    pauseCurrentDatetime = 0;
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Main entry of the SIR firmware
 *
 * All the initialisations before entering the for(;;) loop are done BEFORE
 * the first key is ever pressed. So when entering the Setup (POWER + VOLMIN) some
 * initialisatons need to be done again when leaving the Setup because new values
 * might be current now
 *
 * \return \b never returns
 */
/* ����������������������������������������������������������������������� */
int main(void)
{
    backlightStayOn = 0;
    
    WatchDogDisable();

    NutDelay(100);

    SysInitIO();	
    SPIinit();
    Uart0DriverInit();
    Uart0DriverStart();
    LogInit();
    //Initialize SDCard
    CardInit();
    //Initialize remote control
    RcInit();
    //Initialize keyboard
    KbInit();
    //Initialize LED light
    LedInit();
    //Initialize LCD screen
    LcdLowLevelInit();

    SysControlMainBeat(ON);             // enable 4.4 msecs hartbeat interrupt
    
    /* Enable global interrupts */
    sei();
    
    //Create the backlight thread
    NutThreadCreate("BacklightThread", BacklightThread, NULL, 1024);
    
    //Initialize persistent data chip
    if (At45dbInit()==AT45DB041B)
    {   
        //Read SRAM, starting at page 0. Put the address of firstStartup as a parameter, and the bytesize is a size of an int
        At45dbPageRead(0, &firstStartup, sizeof(int));

        //If debugging is enabled, erase the firstStartup page (pgn 0)
        if(DEBUG)
            firstStartup = 1;

        LogMsg_P(LOG_INFO, PSTR("Value of firstStartup: %d"), firstStartup);
    }
    //Initialize RTC
    X12Init();
    
    //Initialize network
    NetworkInit();
    //timeZone check
    SetTimezone();
    //From now on, set the timezone from the menu
    setTimezoneFromMenu = 1;
    //Try to sync the time and date with an NTP server
    SyncDatetime();
    //Initialize Menu
    MenuInit();
    
    //Do not pause the updating of the current time;
    pauseCurrentDatetime = 0;
    
    if(firstStartup)
    {
        firstStartup = 0;
        At45dbPageWrite(0, &firstStartup, sizeof(int));
        if(DEBUG)
            LogMsg_P(LOG_INFO, PSTR("Value of firstStartup: %d"), firstStartup);
    }
    
    /*
     * Increase our priority so we can feed the watchdog.
     */
    NutThreadSetPriority(1);
    
    for (;;)
    {
        NutSleep(500);
        
        if(!pauseCurrentDatetime)
        {
            //Show the current time
            ShowCurrentTime();
        }
        
        WatchDogRestart();
    }

    return(0);      // never reached, but 'main()' returns a non-void, so.....
}
/* ---------- end of module ------------------------------------------------ */

/*@}*/
