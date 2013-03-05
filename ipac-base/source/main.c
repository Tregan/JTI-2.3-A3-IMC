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

#include <time.h>
#include "rtc.h"
#include "menu.h"


/*-------------------------------------------------------------------------*/
/* global variable definitions                                             */
/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/* local variable definitions                                              */
/*-------------------------------------------------------------------------*/
//Enable or disable debugging messages and functionality (1 = on, 0 = off)
const int DEBUG = 0;

int pauseCurrentTime;
int timeZone;
int timeZoneSet;
int timeSetManually;
int setTimezoneFromMenu;
int selectedTimeUnit;
int alarmAset;
int alarmBset;
//Kroeske: time struct uit nut/os time.h (http://www.ethernut.de/api/time_8h-source.html)
tm datetime;
tm alarmA;
tm alarmB;

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
            if(key == KEY_UP && timeZone <= 13)
                timeZone++;
            else if(key == KEY_DOWN && timeZone >= -11)
                timeZone--;
            else if(key == KEY_OK)
            {
                timeZoneSet = 1;
                //Thread no longer needed, exit please
                NutThreadExit();
            }
        }
    }
}

THREAD(KBThreadManualTime, args)
{
    //Time units: 0 = hours, 1 = minutes, 2 = seconds
    selectedTimeUnit = 0;
    
    for(;;)
    {
        NutSleep(300);
        //Wait for keyboard event
        if(KbWaitForKeyEvent(500) != KB_ERROR)
        {
            u_char key = KbGetKey();
            if(key == KEY_UP)
            {
                switch(selectedTimeUnit)
                {
                    case 0:
                        if(datetime.tm_hour >= 23)
                            datetime.tm_hour = 0;
                        else
                            datetime.tm_hour++;
                        break;
                    case 1:
                        if(datetime.tm_min >= 59)
                            datetime.tm_min = 0;
                        else
                            datetime.tm_min++;
                        break;
                    case 2:
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
                switch(selectedTimeUnit)
                {
                    case 0:
                        if(datetime.tm_hour <= 0)
                            datetime.tm_hour = 23;
                        else
                            datetime.tm_hour--;
                        break;
                    case 1:
                        if(datetime.tm_min <= 0)
                            datetime.tm_min = 59;
                        else
                            datetime.tm_min--;
                        break;
                    case 2:
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
                if(selectedTimeUnit <= 0)
                {
                    selectedTimeUnit = 2;
                }
                else
                    selectedTimeUnit--;
            }
            else if(key == KEY_RIGHT)
            {
                //Set to hours if it already is at seconds
                if(selectedTimeUnit >= 2)
                {
                    selectedTimeUnit = 0;
                }
                else
                    selectedTimeUnit++;
            }
            else if(key == KEY_OK)
            {
                timeSetManually = 1;
                //Thread no longer needed, exit please
                NutThreadExit();
            }
        }
    }
}

/*!
 * \brief setting Alarm A thread
 * 
 * even kijken of dit zo blijft of dat het makkelijker kan, (dit is eigenlijk precies het zelfde als de set time)
 * 
 * \author Bas, Matthijs
 */
THREAD(SetAlarmAThread, args)
{
    selectedTimeUnit = 0;
    
    for(;;)
    {
        NutSleep(300);
        //Wait for keyboard event
        if(KbWaitForKeyEvent(500) != KB_ERROR)
        {
            u_char key = KbGetKey();
            if(key == KEY_UP)
            {
                switch(selectedTimeUnit)
                {
                    case 0:
                        if(alarmA.tm_hour >= 23)
                            alarmA.tm_hour = 0;
                        else
                            alarmA.tm_hour++;
                        break;
                    case 1:
                        if(alarmA.tm_min >= 59)
                            alarmA.tm_min = 0;
                        else
                            alarmA.tm_min++;
                        break;
                    case 2:
                        if(alarmA.tm_sec >= 59)
                            alarmA.tm_sec = 0;
                        else
                            alarmA.tm_sec++;
                        break;
                    default:
                        break;
                }
            }
            else if(key == KEY_DOWN)
            {
                switch(selectedTimeUnit)
                {
                    case 0:
                        if(alarmA.tm_hour <= 0)
                            alarmA.tm_hour = 23;
                        else
                            alarmA.tm_hour--;
                        break;
                    case 1:
                        if(alarmA.tm_min <= 0)
                            alarmA.tm_min = 59;
                        else
                            alarmA.tm_min--;
                        break;
                    case 2:
                        if(alarmA.tm_sec <= 0)
                            alarmA.tm_sec = 59;
                        else
                            alarmA.tm_sec--;
                        break;
                    default:
                        break;
                }
            }
            else if(key == KEY_LEFT)
            {
                //Set to seconds if it already is at hours
                if(selectedTimeUnit <= 0)
                {
                    selectedTimeUnit = 2;
                }
                else
                    selectedTimeUnit--;
            }
            else if(key == KEY_RIGHT)
            {
                //Set to hours if it already is at seconds
                if(selectedTimeUnit >= 2)
                {
                    selectedTimeUnit = 0;
                }
                else
                    selectedTimeUnit++;
            }
            else if(key == KEY_OK)
            {
                alarmAset = 1;
                //Thread no longer needed, exit please
                NutThreadExit();
            }
        }
    }
}

/*!
 * \brief setting Alarm B thread
 * 
 * aangevuld met de datum door Matthijs
 * 
 * \author Bas, Matthijs
 */
THREAD(SetAlarmBThread, args)
{
    selectedTimeUnit = 0;
    
    for(;;)
    {
        NutSleep(300);
        //Wait for keyboard event
        if(KbWaitForKeyEvent(500) != KB_ERROR)
        {
            u_char key = KbGetKey();
            if(key == KEY_UP)
            {
                switch(selectedTimeUnit)
                {
                    case 0:
                        if(alarmB.tm_mon >= 11)
                            alarmB.tm_mon = 0;
                        else
                            alarmB.tm_mon++;
                        break;
                    case 1:
                        if(alarmB.tm_mday >= 31)
                            alarmB.tm_mday = 1;
                        else
                            alarmB.tm_mday++;
                        break;
                    case 2:
                        if(alarmB.tm_hour >= 23)
                            alarmB.tm_hour = 0;
                        else
                            alarmB.tm_hour++;
                        break;
                    case 3:
                        if(alarmB.tm_min >= 59)
                            alarmB.tm_min = 0;
                        else
                            alarmB.tm_min++;
                        break;
                    case 4:
                        if(alarmB.tm_sec >= 59)
                            alarmB.tm_sec = 0;
                        else
                            alarmB.tm_sec++;
                        break;
                    default:
                        break;
                }
            }
            else if(key == KEY_DOWN)
            {
                switch(selectedTimeUnit)
                {
                    case 0:
                        if(alarmB.tm_mon <= 0)
                            alarmB.tm_mon = 11;
                        else
                            alarmB.tm_mon--;
                        break;
                    case 1:
                        if(alarmB.tm_mday <= 1)
                            alarmB.tm_mday = 31;
                        else
                            alarmB.tm_mday--;
                        break;
                    case 2:
                        if(alarmB.tm_hour <= 0)
                            alarmB.tm_hour = 23;
                        else
                            alarmB.tm_hour--;
                        break;
                    case 3:
                        if(alarmB.tm_min <= 0)
                            alarmB.tm_min = 59;
                        else
                            alarmB.tm_min--;
                        break;
                    case 4:
                        if(alarmB.tm_sec <= 0)
                            alarmB.tm_sec = 59;
                        else
                            alarmB.tm_sec--;
                        break;
                    default:
                        break;
                }
            }
            else if(key == KEY_LEFT)
            {
                if(selectedTimeUnit <= 0)
                {
                    selectedTimeUnit = 4;
                }
                else
                    selectedTimeUnit--;
            }
            else if(key == KEY_RIGHT)
            {
                if(selectedTimeUnit >= 4)
                {
                    selectedTimeUnit = 0;
                }
                else
                    selectedTimeUnit++;
            }
            else if(key == KEY_OK)
            {
                alarmBset = 1;
                //Thread no longer needed, exit please
                NutThreadExit();
            }
        }
    }
}

//ThreadA for turning on LED
THREAD(ThreadA, args)
{
    for(;;)
    {
        NutSleep(500);
        LedControl(LED_POWER_ON);
    }
}

//ThreadB for turning off LED
THREAD(ThreadB, args)
{
    for(;;)
    {
        NutSleep(500);
        LedControl(LED_POWER_OFF);
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Set _timezone of time.h
 * \author Niels & Bas
 */
/* ����������������������������������������������������������������������� */
void setTimeZone(int* timeZone)
{
    if((-12 <= *timeZone) && (*timeZone <= 14))
    {
        printf("\ntimezone = %d", *timeZone);
        _timezone = -*timeZone * 60 * 60;
    }
    else
    {
        _timezone = 0;
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Starts the check for firstStartup, and if so: waits for timeZone input
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
void InitializeTimeZone(void)
{
    //Clear display
    LcdClearAll();
    
    int firstStartup;
    //Read SRAM, starting at page 0. Put the address of firstStartup as a parameter, and the bytesize is a size of an int
    At45dbPageRead(0, &firstStartup, sizeof(int));
    
    //If debugging is enabled, erase the firstStartup page (pgn 0)
    if(DEBUG == 1)
    {
        firstStartup = 1;
        LogMsg_P(LOG_INFO, PSTR("Value of firstStartup: %d"), firstStartup);
    }

    //First startup or called from menu, set the timezone!
    if(firstStartup || setTimezoneFromMenu)
    {
        if(DEBUG)
            LogMsg_P(LOG_INFO, PSTR("Waiting for timeZone input"));

        LcdBackLight(LCD_BACKLIGHT_ON);
        LcdWriteTitle("Set Timezone");
        timeZoneSet = 0;
        if(firstStartup)
            timeZone = 0;
        else
            timeZone = -(_timezone /60) / 60;
        
        //Create the Keyboard Thread for setting the timeZone
        NutThreadCreate("KBThreadTimeZone", KBThreadTimeZone, NULL, 1024);

        while(timeZoneSet != 1)
        {
            NutSleep(100);

            //array of 20 chars should be enough for "UTC +(or -)14"
            char output[20];
            if(timeZone > -1)
                sprintf(output, "UTC +%d ", timeZone);
            else
                sprintf(output, "UTC %d ", timeZone);
            LcdWriteSecondLine(output);

            WatchDogRestart();
        }

        LcdBackLight(LCD_BACKLIGHT_OFF);
        
        if(firstStartup)
        {
            firstStartup = 0;
            At45dbPageWrite(0, &firstStartup, sizeof(int));
            if(DEBUG)
                LogMsg_P(LOG_INFO, PSTR("Value of firstStartup: %d"), firstStartup);
        }
        
        //Time changed, because timezone changed
        if(setTimezoneFromMenu)
        {
            int test = timeZone - (-(_timezone / 60) / 60);
            datetime.tm_hour += test;
            
            //Write the gmt struct to RTC
            X12RtcSetClock(&datetime);
        }
        
        At45dbPageWrite(0 + sizeof(int), &timeZone, sizeof(int));
    }
    else if(!firstStartup)
    {
        //Start at page sizeof(int), because that's the bytesize of firstStartup, which starts at page 0
        //Put the address of timeZone as a parameter, and the bytesize is the size of a float
        At45dbPageRead(0 + sizeof(int), &timeZone, sizeof(int));
        if(DEBUG)
            LogMsg_P(LOG_INFO, PSTR("timeZone written to SRAM with value %d"), timeZone);
    }
    
    //Clear the display
    LcdClearAll();

    //Set timezone of time.h to our timezone
    setTimeZone(&timeZone);
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
 * \brief Manually set the time
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
void setTimeManually(void)
{
    //While setting the time, we do not want to see it updated, undoing our changes
    pauseCurrentTime = 1;
    
    //Get the current time from RTC and store it in gmt struct
    X12RtcGetClock(&datetime);
            
    timeSetManually = 0;
    
    //Create the Keyboard Thread for setting the timeZone
    NutThreadCreate("KBThreadManualTime", KBThreadManualTime, NULL, 1024);
    
    //Backlight on during setting
    LcdBackLight(LCD_BACKLIGHT_ON);
    //Clear the title
    LcdClearAll();
    LcdWriteTitle("Set Time Manually");
    
    //Show the current time, but do NOT update it! We're setting a time, updating while setting would be very silly
    ShowCurrentTime();
    
    char output[20];
    
    while(timeSetManually != 1)
    {
        NutSleep(100);
        
        //Clear the second line
        LcdClearLine();
        
        switch(selectedTimeUnit)
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
    //Backlight no longer needed, turn off
    LcdBackLight(LCD_BACKLIGHT_OFF);
    //Write the gmt struct to RTC
    X12RtcSetClock(&datetime);
    //Done, start updating the current time again
    pauseCurrentTime = 0;
}

/*!
 * \brief set Alarm A
 * \author Matthijs, Bas
 */
void AlarmAMenu(void)
{
    int flags;
    //alarm ophalen
    X12RtcGetAlarm(0,&alarmA, &flags);  
    alarmAset = 0;
    
    //key listener starten
    NutThreadCreate("SetAlarmAThread", SetAlarmAThread, NULL, 1024);
    
    char output[20];
    //Backlight on during setting
    LcdBackLight(LCD_BACKLIGHT_ON);
    //Clear the title
    LcdClearAll();
    LcdWriteTitle("Set Alarm A");
    
    ShowCurrentTime();
    
    while(alarmAset != 1)
    {
        NutSleep(100);
        
        //Clear the second line
        LcdClearLine();
        
        switch(selectedTimeUnit)
        {
            case 0:
                sprintf(output, "Set Hours: %02d", alarmA.tm_hour);
                LcdWriteSecondLine(output);
                break;
            case 1:
                sprintf(output, "Set Minutes: %02d", alarmA.tm_min);
                LcdWriteSecondLine(output);
                break;
            case 2:
                sprintf(output, "Set Seconds: %02d", alarmA.tm_sec);
                LcdWriteSecondLine(output);
                break;
            default:
                break;
        }
    }
    
    LcdClearAll();
    //Backlight no longer needed, turn off
    LcdBackLight(LCD_BACKLIGHT_OFF);
    //set Alarm A
    X12RtcSetAlarm(0,&alarmA,7);
}

/*!
 * \brief set Alarm B
 * \author Matthijs, Bas
 */
void AlarmBMenu(void)
{
    int flags;
    //alarm ophalen
    X12RtcGetAlarm(1,&alarmB, &flags);  
    alarmBset = 0;
    
    //key listener starten
    NutThreadCreate("SetAlarmBThread", SetAlarmBThread, NULL, 1024);
    
    char output[20];
    //Backlight on during setting
    LcdBackLight(LCD_BACKLIGHT_ON);
    //Clear the title
    LcdClearAll();
    LcdWriteTitle("Set Alarm B");
    
    ShowCurrentTime();
    
    while(alarmBset != 1)
    {
        NutSleep(100);
        
        //Clear the second line
        LcdClearLine();
        
        switch(selectedTimeUnit)
        {
            case 0:
                sprintf(output, "Set Month: %02d", alarmB.tm_mon);
                LcdWriteSecondLine(output);
                break;
            case 1:
                sprintf(output, "Set Day: %02d", alarmB.tm_mday);
                LcdWriteSecondLine(output);
                break;
            case 2:
                sprintf(output, "Set Hour: %02d", alarmB.tm_hour);
                LcdWriteSecondLine(output);
                break;
            case 3:
                sprintf(output, "Set Minutes: %02d", alarmB.tm_min);
                LcdWriteSecondLine(output);
                break;
             case 4:
                sprintf(output, "Set Seconds: %02d", alarmB.tm_sec);
                LcdWriteSecondLine(output);
                break;
            default:
                break;
        }
    }
    
    LcdClearAll();
    //Backlight no longer needed, turn off
    LcdBackLight(LCD_BACKLIGHT_OFF);
    //set Alarm B
    X12RtcSetAlarm(1,&alarmB,31);
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
    /*
     *  First disable the watchdog
     */
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
    
    //Initialize persistent data chip
    if (At45dbInit()==AT45DB041B)
    {      
        //timeZone check
        //InitializeTimeZone();
        //From now on, set the timezone from the menu
        //setTimezoneFromMenu = 1;
    }
    
    //Initialize RTC
    X12Init();
    
    //Initialize network
    NetworkInit();
    
    //Query NTP server and set time. If no time was set, set it manually.
    if(!NTP(&datetime))
    {
        LcdWriteTitle("Failed.");
        LcdWriteSecondLine("No time obtained");
        NutSleep(2000);
        setTimeManually();
    }
    
    //Initialize Menu
    MenuInit();
    
    //Do not pause the updating of the current time;
    pauseCurrentTime = 0;
    
    /*
     * Increase our priority so we can feed the watchdog.
     */
    NutThreadSetPriority(1);
    
    NutThreadCreate("MainA", ThreadA, NULL, 1024);
    NutThreadCreate("MainB", ThreadB, NULL, 1024);
    
    int count = 0;
    for (;;)
    {
        NutSleep(500);
        u_char key = KbGetKey();
        if(key != KEY_UNDEFINED)
        {
            count = 0;
            LcdBackLight(LCD_BACKLIGHT_ON);
        }
        
        count++;
        
        if(count >= 20)
            LcdBackLight(LCD_BACKLIGHT_OFF);
        
        if(!pauseCurrentTime)
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
