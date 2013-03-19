/* ========================================================================
 * [PROJECT]    SIR
 * [MODULE]     Real Time Clock
 * [TITLE]      High- and low level Routines for INtersil X1205 RTC chip
 * [FILE]       rtc.c
 * [VSN]        1.0
 * [CREATED]    13042007
 * [LASTCHNGD]  131042007
 * [COPYRIGHT]  Copyright (C) STREAMIT BV 2010
 * [PURPOSE]    contains all interface- and low-level routines to
 *              read/write date/time/status strings from the X1205
 * ======================================================================== */

#define LOG_MODULE  LOG_RTC_MODULE

#include <cfg/os.h>
#include <dev/twif.h>
#include <sys/event.h>
#include <sys/timer.h>

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "rtc.h"
#include "portio.h"
#include "vs10xx.h"
#include "flash.h"

#define I2C_SLA_RTC         0x6F
#define I2C_SLA_EEPROM      0x57
#define EEPROM_PAGE_SIZE    64


static u_long rtc_status;
tm alarmA;

/*!
 * \brief thread that checks the alarms every 10 seconds
 * 
 * \author Matthijs
 */
THREAD(AlarmThread, args)
{
	//time struct datetime can't be found when placed in main.h
	tm datetime;

    u_long flags;
    X12RtcGetClock(&datetime);

    for(;;)
    {
        int succes = X12RtcGetStatus(&flags);
        int i;
        
        for(i = 0; i <=5; i++)
        {
            NutSleep(1000);
        }
        
        //power fail
        if(flags == 0)
        {
            printf("\n ========== Power Fail ========== \n");
            X12RtcClearStatus(0);
        }

        //alarm A
        if(flags == 32)
        {
            printf("\n ====================================================== Alarm A =========== \n");


            //get current alarm time to compare to the weekendtime, if weekendtime is
            //equal to currentAlarm and the day of the week is friday. The alarm should
            //not go off.
            tm currentAlarm;
            int iets = 0;
            X12RtcGetAlarm(0, &currentAlarm, &iets);

            int alarmASeconds = 0;
            alarmASeconds = (currentAlarm.tm_hour * 360) + (currentAlarm.tm_min * 6) + (currentAlarm.tm_sec / 10);

            int weekendtimeSeconds = 0;
            weekendtimeSeconds = (weekendtime.hour * 360) + (weekendtime.minute * 6) + (weekendtime.second / 10);


            printf("\nalarmASeconds: %d, weekendtimeSeconds: %d", alarmASeconds, weekendtimeSeconds);
            if((alarmASeconds != weekendtimeSeconds) && checkWeekend() == 1)
            {
                printf("\nHet is VRIJDAG!!! alarm is niet gelijk aan de weekendtijd en moet dus nog geset worden\n SOUND IS PLAYING");
                //SoundA();
                startSnoozeThreadA();
            }
            else if(((alarmASeconds != weekendtimeSeconds) && (checkWeekend() == 2) || ((alarmASeconds != weekendtimeSeconds) && (checkWeekend() == 3))))
            {
                printf("\nalarm is niet gelijk aan de weekendtijd en het is weekend, dus set");
            }
            else if(((alarmASeconds == weekendtimeSeconds) && (checkWeekend() == 2) || ((alarmASeconds == weekendtimeSeconds) && (checkWeekend() == 3))))
            {
                printf("\nalarm is gelijk aan de weekendtijd en het is weekend,\n SOUND IS PLAYING");
                //SoundA();
                startSnoozeThreadA();

                //if the alarm goes off and it is sunday set the alarm back to its
                //original settings
                if(checkWeekend() == 3)
                {
                    printf("het is blijkbaar zondag dus terug naar doordeweekse tijd");
                    setAlarmA(alarmA.tm_hour, alarmA.tm_min, alarmA.tm_sec);
                }
            }
            else if(checkWeekend() == 0)
            {
                printf("\nhet is doordeweeks,\nSOUND IS PLAYING");
                startSnoozeThreadA();
                //SoundA();
            }
            //else
            //{
            //	printf("weekendAlarm gaat af!!!");
            //}

            //when checkWeekend returns 1(Friday) or 2(Saturday) it means that it
            //will be weekend the next day. Therefore we set the alarm to the weekend
            //settings. Problem is that most likely the alarm in the weekends will go
            //off later then it would during the week. This problem is solved underneath
            //the if(checkWeekend() == 3){}.
            int currentTime = (datetime.tm_hour * 360) + (datetime.tm_min * 6) + (datetime.tm_sec / 10);

            if(checkWeekend() > 0 && checkWeekend() < 3)
            {
                printf("alarmA set to weekend time");
                setAlarmA(weekendtime.hour,weekendtime.minute,weekendtime.second);
            }
            else if(checkWeekend() == 3 && (currentTime <= weekendtimeSeconds))
            {
                setAlarmA(weekendtime.hour,weekendtime.minute,weekendtime.second);
            }


            X12RtcClearStatus(32);
        }

        //alarm B
        if(flags == 64)
        {
            printf("\n ============ Alarm 1================== \n");
            startSnoozeThreadB();
            X12RtcClearStatus(64);
            alarmBArray[currentAlarm.index].set = 0;
            alarmBStruct newSet = checkFirst();
            X12RtcSetAlarm(1, &newSet.timeSet, 31);
            currentAlarm = newSet;
        }

        //both alarms
        if(flags == 96)
        {
            //kijkt of alarm a of b nog niet heeft geluid, en speelt die dan
            printf("\n ========== Alarm 0 en 1  ========= \n");
            //kijkt welke van de 2 nog niet is afgegaan en speelt die dan nog af
            SoundA();  
            X12RtcClearStatus(32);
            SoundB();
            X12RtcClearStatus(64);
        }

        if(succes == 0)
        {
            //succes
        }
        else
        {
            //error
        }
    }
}

/*!
 * \brief Enable or disable write access.
 *
 * \param on Write access is disabled if this parameter is 0, or
 *           enabled otherwise.
 *
 * \return 0 on success or -1 in case of an error.
 */
static int X12WriteEnable(int on)
{
    int rc;
    u_char buf[3];

    buf[0] = 0;
    buf[1] = 0x3F;
    if (on)
    {
        buf[2] = 0x02;
        if ((rc = TwMasterTransact(I2C_SLA_RTC, buf, 3, 0, 0, NUT_WAIT_INFINITE)) == 0)
        {
            buf[2] = 0x06;
            rc = TwMasterTransact(I2C_SLA_RTC, buf, 3, 0, 0, NUT_WAIT_INFINITE);
        }
    }
    else
    {
        buf[2] = 0x00;
        rc = TwMasterTransact(I2C_SLA_RTC, buf, 3, 0, 0, NUT_WAIT_INFINITE);
    }
    return(rc);
}

/*!
 * \brief Wait until non-volatile write cycle finished.
 *
 * \return 0 on success or -1 in case of an error.
 */
static int X12WaitReady(void)
{
    u_char poll;
    int cnt = 20;

    /* Poll for write cycle finished. */
    while (--cnt && TwMasterTransact(I2C_SLA_EEPROM, 0, 0, &poll, 1, NUT_WAIT_INFINITE) == -1)
    {
        NutSleep(1);
    }
    return(cnt ? 0 : -1);
}

/*!
 * \brief Read RTC registers.
 *
 * \param reg  The first register to read.
 * \param buff Pointer to a buffer that receives the register contents.
 * \param cnt  The number of registers to read.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12RtcReadRegs(u_char reg, u_char *buff, size_t cnt)
{
    int rc = -1;
    u_char wbuf[2];

    wbuf[0] = 0;
    wbuf[1] = reg;
    if (TwMasterTransact(I2C_SLA_RTC, wbuf, 2, buff, cnt, NUT_WAIT_INFINITE) == cnt)
    {
        rc = 0;
    }
    return(rc);
}

/*!
 * \brief Write to RTC registers.
 *
 * \param nv   Must be set to 1 when writing to non-volatile registers.
 *             In this case the routine will poll for write cycle
 *             completion before returning to the caller. Set to zero
 *             if writing to volatile registers.
 * \param buff This buffer must contain all bytes to be transfered to
 *             the RTC chip, including the register address.
 * \param cnt  Number of valid bytes in the buffer.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12RtcWrite(int nv, CONST u_char *buff, size_t cnt)
{
    int rc;

    if ((rc = X12WriteEnable(1)) == 0)
    {
        rc = TwMasterTransact(I2C_SLA_RTC, buff, cnt, 0, 0, NUT_WAIT_INFINITE);
        if (rc == 0 && nv)
        {
            rc = X12WaitReady();
        }
        X12WriteEnable(0);
    }
    return(rc);
}

/*!
 * \brief Get date and time from an X12xx hardware clock.
 *
 * \deprecated New applications must use NutRtcGetTime().
 *
 * \param tm Points to a structure that receives the date and time
 *           information.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12RtcGetClock(struct _tm *tm)
{
    int rc;
    u_char data[8];

    if ((rc = X12RtcReadRegs(X12RTC_SC, data, 8)) == 0)
    {
        tm->tm_sec = BCD2BIN(data[0]);
        tm->tm_min = BCD2BIN(data[1]);
        tm->tm_hour = BCD2BIN(data[2] & 0x3F);
        tm->tm_mday = BCD2BIN(data[3]);
        tm->tm_mon = BCD2BIN(data[4]) - 1;
        tm->tm_year = BCD2BIN(data[5]) + 100;
        if (BCD2BIN(data[7]) > 0x19)
        {
            tm->tm_year += 100;
        }
        tm->tm_wday = data[6];
    }
    return(rc);
}

/*!
 * \brief Set an X12xx hardware clock.
 *
 * \deprecated New applications must use NutRtcSetTime().
 *
 * New time will be taken over at the beginning of the next second.
 *
 * \param tm Points to a structure which contains the date and time
 *           information.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12RtcSetClock(CONST struct _tm *tm)
{
    u_char data[10];

    memset(data, 0, sizeof(data));
    if (tm)
    {
        data[1] = X12RTC_SC;
        data[2] = BIN2BCD(tm->tm_sec);
        data[3] = BIN2BCD(tm->tm_min);
        data[4] = BIN2BCD(tm->tm_hour) | 0x80;
        data[5] = BIN2BCD(tm->tm_mday);
        data[6] = BIN2BCD(tm->tm_mon + 1);
        if (tm->tm_year > 99)
        {
            data[7] = BIN2BCD(tm->tm_year - 100);
            data[9] = 0x20;
        }
        else
        {
            data[7] = BIN2BCD(tm->tm_year);
            data[9] = 0x19;
        }
        data[8] = tm->tm_wday;
    }
    return(X12RtcWrite(0, data, 10));
}

/*!
 * \brief Get alarm date and time of an X12xx hardware clock.
 *
 * \deprecated New applications must use NutRtcGetAlarm().
 *
 * \param idx   Zero based index. Two alarms are supported.
 * \param tm    Points to a structure that receives the date and time
 *              information.
 * \param aflgs Points to an unsigned long that receives the enable flags.
 *
 * \return 0 on success or -1 in case of an error.
 *
 */
int X12RtcGetAlarm(int idx, struct _tm *tm, int *aflgs)
{
    int rc;
    u_char data[8];

    *aflgs = 0;
    memset(tm, 0, sizeof(struct _tm));
    if ((rc = X12RtcReadRegs(idx * 8, data, 8)) == 0)
    {
        if (data[0] & X12RTC_SCA_ESC)
        {
            *aflgs |= RTC_ALARM_SECOND;
            tm->tm_sec = BCD2BIN(data[0] & 0x7F);
        }
        if (data[1] & X12RTC_MNA_EMN)
        {
            *aflgs |= RTC_ALARM_MINUTE;
            //Matthijs: Added: 0x70, seems logical with 0 to 59
            tm->tm_min = BCD2BIN(data[1] & 0x7F);
        }
        if (data[2] & X12RTC_HRA_EHR)
        {
            *aflgs |= RTC_ALARM_HOUR;
            tm->tm_hour = BCD2BIN(data[2] & ~0x80);
        }
        if (data[3] & X12RTC_DTA_EDT)
        {
            *aflgs |= RTC_ALARM_MDAY;
            //Matthijs: Added: 0x70, not logical
            tm->tm_mday = BCD2BIN(data[3] & 0x7F);
        }
        if (data[4] & X12RTC_MOA_EMO)
        {
            *aflgs |= RTC_ALARM_MONTH;
            //Matthijs: Added: 0x70, not logical
            tm->tm_mon = BCD2BIN(data[4] & 0x7F) - 1;
        }
        if (data[6] & X12RTC_DWA_EDW)
        {
            *aflgs |= RTC_ALARM_WDAY;
            tm->tm_wday = BCD2BIN(data[6]);
        }
    }
    return(rc);
}
tm test;
/*!
 * \brief Set alarm of an X12xx hardware clock.
 *
 * \deprecated New applications must use NutRtcSetAlarm().
 *
 * \param idx   Zero based index. Two alarms are supported.
 * \param tm    Points to a structure which contains the date and time
 *              information. May be NULL to clear the alarm.
 * \param aflgs Each bit enables a specific comparision.
 *              - Bit 0: Seconds
 *              - Bit 1: Minutes
 *              - Bit 2: Hours
 *              - Bit 3: Day of month
 *              - Bit 4: Month
 *              - Bit 7: Day of week (Sunday is zero)
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12RtcSetAlarm(int idx, CONST struct _tm *tm, int aflgs)
{
    u_char data[10];
    int flags;
    
    
    memset(data, 0, sizeof(data));
    data[1] = idx * 8;
    if (tm)
    {
        if (aflgs & RTC_ALARM_SECOND)
        {
            data[2] = BIN2BCD(tm->tm_sec) | X12RTC_SCA_ESC;
        }
        if (aflgs & RTC_ALARM_MINUTE)
        {
            data[3] = BIN2BCD(tm->tm_min) | X12RTC_MNA_EMN;
        }
        if (aflgs & RTC_ALARM_HOUR)
        {
            data[4] = BIN2BCD(tm->tm_hour) | X12RTC_HRA_EHR;
        }
        if (aflgs & RTC_ALARM_MDAY)
        {
            data[5] = BIN2BCD(tm->tm_mday) | X12RTC_DTA_EDT;
        }
        if (aflgs & RTC_ALARM_MONTH)
        {
            data[6] = BIN2BCD(tm->tm_mon + 1) | X12RTC_MOA_EMO;
        }
        if (aflgs & RTC_ALARM_WDAY)
        {
            data[8] = BIN2BCD(tm->tm_wday) | X12RTC_DWA_EDW;
        }
    }
    X12RtcGetAlarm(1, &test, &flags);
    //printf("\n set Alarm: date: %d-%d-%d Time: %d:%d:%d \n", test.tm_year, test.tm_mon, test.tm_mday, test.tm_hour, test.tm_min, test.tm_sec);
    return(X12RtcWrite(1, data, 10));
}

/*!
 * \brief Query RTC status flags.
 *
 * \deprecated New applications must use NutRtcGetStatus().
 *
 * \param sflgs Points to an unsigned long that receives the status flags.
 *              - Bit 0: Power fail.
 *              - Bit 5: Alarm 0 occured.
 *              - Bit 6: Alarm 1 occured.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12RtcGetStatus(u_long *sflgs)
{
    int rc;
    u_char data;

    if ((rc = X12RtcReadRegs(X12RTC_SR, &data, 1)) == 0)
    {
        rtc_status |= data;
        *sflgs = rtc_status;
    }
    return(rtc_status);
}

/*!
 * \brief Clear RTC status flags.
 *
 * \deprecated New applications must use NutRtcClearStatus().
 *
 * \param sflgs Status flags to clear.
 *
 * \return Always 0.
 */
int X12RtcClearStatus(u_long sflgs)
{
    rtc_status &= ~sflgs;

    return(0);
}



/*!
 * \brief Read contents from non-volatile EEPROM.
 *
 * \param addr  Start location.
 * \param buff  Points to a buffer that receives the contents.
 * \param len   Number of bytes to read.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12EepromRead(u_int addr, void *buff, size_t len)
{
    int rc = -1;
    u_char wbuf[2];

    wbuf[0] = (u_char)(addr >> 8);
    wbuf[1] = (u_char)addr;
    if (TwMasterTransact(I2C_SLA_EEPROM, wbuf, 2, buff, len, NUT_WAIT_INFINITE) == len)
    {
        rc = 0;
    }
    return(rc);
}

/*!
 * \brief Store buffer contents in non-volatile EEPROM.
 *
 * The EEPROM of the X122x has a capacity of 512 bytes, while the X1286 is
 * able to store 32 kBytes.
 *
 * \param addr  Storage start location.
 * \param buff  Points to a buffer that contains the bytes to store.
 * \param len   Number of valid bytes in the buffer.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12EepromWrite(u_int addr, CONST void *buff, size_t len)
{
    int rc = 0;
    u_char *wbuf;
    size_t wlen;
    CONST u_char *wp = buff;

    /*
     * Loop for each page to be written to.
     */
    while (len)
    {
        /* Do not cross page boundaries. */
        wlen = EEPROM_PAGE_SIZE - (addr & (EEPROM_PAGE_SIZE - 1));
        if (wlen > len)
        {
            wlen = len;
        }

        /* Allocate and set a TWI write buffer. */
        if ((wbuf = malloc(wlen + 2)) == 0)
        {
            rc = -1;
            break;
        }
        wbuf[0] = (u_char)(addr >> 8);
        wbuf[1] = (u_char)addr;
        memcpy(wbuf + 2, (void *)wp, wlen);

        /* Enable EEPROM write access and send the write buffer. */
        if ((rc = X12WriteEnable(1)) == 0)
        {
            rc = TwMasterTransact(I2C_SLA_EEPROM, wbuf, wlen + 2, 0, 0, NUT_WAIT_INFINITE);
        }

        /* Release the buffer and check the result. */
        free(wbuf);
        if (rc)
        {
            break;
        }
        len -= wlen;
        addr += wlen;
        wp += wlen;

        /* Poll for write cycle finished. */
        if ((rc = X12WaitReady()) != 0)
        {
            break;
        }
    }
    X12WriteEnable(0);

    return(rc);
}

/*
 * \brief save the alarms to sram
 * 
 * \author Matthijs

void save(void)
{
    //write the alarmBArray to the sram
    At45dbPageWrite(5, &alarmBArray, sizeof(alarmBArray));
}
 */
/*
 * \brief load the alarms from the sram
 * 
 * \author Matthijs
 
void load(void)
{
    //read the alarmBArray from the sram
    At45dbPageRead(5,&alarmBArray, sizeof(alarmBArray));
}
*/
/*
 * \brief setting the alarms at a initiallizer
 * 
 * \author Matthijs
 */
void createAlarms(void)
{   
    tm startTime;
    startTime.tm_year = 0;
    startTime.tm_mon = 0;
    startTime.tm_mday = 0;
    startTime.tm_hour = 0;
    startTime.tm_min = 0;
    startTime.tm_sec = 0;
    alarmBStruct test;
    int i;
    for (i = 0; i <=10; i++)
    {        
        test.index = i;
        test.timeSet = startTime;
        test.set = 0;
        alarmBArray[i] = test;
    }
}

/*!
 * \brief Initialize the interface to an Intersil X12xx hardware clock.
 *
 * \deprecated New applications must use NutRegisterRtc().
 *
 * \return 0 on success or -1 in case of an error.
 *
 */
int X12Init(void)
{
    int rc;
    u_long tmp;

    if ((rc = TwInit(0)) == 0)
    {
        rc = X12RtcGetStatus(&tmp);
    }
    
    // loading ands setting the alarm
    createAlarms();
    //load();
    alarmBStruct first = checkFirst();
    currentAlarm = first;
    X12RtcSetAlarm(1, &first.timeSet, 31);
    startAlarmThread();
    return (rc);
}

/*!
 * \brief sets AlarmA
 * 
 * \param hours        : 0 - 23 hours from midnight
 * 
 * \param minutes      : 0 - 59 minutes in hour
 * 
 * \param seconds     : 0 - 59 seconds in minutes
 * 
 * \author Matthijs
 */
void setAlarmA(int hours, int minutes, int seconds)
{
    tm alarmA;
    
    alarmA.tm_hour = hours;
    alarmA.tm_min = minutes;
    alarmA.tm_sec = seconds;
    
    // flags 7 becaus that is sec, min, hours in binary
    int succes = X12RtcSetAlarm(0, &alarmA, 7);
    
    if(succes == 0)
    {
          //goed
    }
    else
    {
        //fout
    }
}

/*
 * \brief returns the first alarm That needs to be set
 * 
 * \author Matthijs
 */
alarmBStruct checkFirst(void)
{
    int i;
    alarmBStruct first;
    for(i = 0; i <=10; i++)
    {
        if(alarmBArray[i].set == 1)
        {
                first = alarmBArray[i];
                break;
        }
    }
    
    for(i = 0; i<= 9; i++)
    {
        printf("CheckFirst ==  Alarm %d date: %d-%d-%d time: %d:%d:%d set= %d \n", alarmBArray[i].index, alarmBArray[i].timeSet.tm_year, alarmBArray[i].timeSet.tm_mon, alarmBArray[i].timeSet.tm_mday, alarmBArray[i].timeSet.tm_hour, alarmBArray[i].timeSet.tm_min, alarmBArray[i].timeSet.tm_sec, alarmBArray[i].set);
        if(alarmBArray[i].set != 1)
        {
           continue;
        }
        if(first.timeSet.tm_year > alarmBArray[i].timeSet.tm_year)
        {
            first = alarmBArray[i];
        }
        else if(first.timeSet.tm_year == alarmBArray[i].timeSet.tm_year)
        {
            if(first.timeSet.tm_yday > alarmBArray[i].timeSet.tm_yday)
            {  
                 first = alarmBArray[i];
            }
            else if(first.timeSet.tm_yday == alarmBArray[i].timeSet.tm_yday)
            {
                int a;
                int b;
                a = (first.timeSet.tm_hour *360) + (first.timeSet.tm_min * 6) + (first.timeSet.tm_sec /10);
                b = (alarmBArray[i].timeSet.tm_hour *360) + (alarmBArray[i].timeSet.tm_min * 6) + (alarmBArray[i].timeSet.tm_sec /10);
                if(b < a)
                {
                    first = alarmBArray[i];
                }
            }
        }
    }
    return first; 
}

/*!
 * \brief sets AlarmB
 * 
 * \param alarm         : alarmBStruct of a alarm to set
 * 
 * \param index         : the index of the alarm
 * 
 * \author Matthijs
 */
void setAlarmB(alarmBStruct alarm, int index)
{
    alarmBArray[index] = alarm;
    //////////////////////////////////////////////////////////////////////////////////////////////
    //save();
    alarmBStruct toSet = checkFirst();
    currentAlarm = toSet;
    X12RtcSetAlarm(1, &toSet.timeSet, 31);
    
}

/*!
 * \brief Clears AlarmA or AlarmB
 * 
 * \param ID     A or B - witch alarm must be cleared
 * 
 * \author Matthijs
 */
void ClearAlarm(char ID)
{
    if(ID == 'a')
    {
        X12RtcSetAlarm(0,NULL, 63);
    }
    
    if(ID == 'b')
    {
        X12RtcSetAlarm(1,NULL, 63);
    }
}



/*!
 * \brief start the alarm check thread
 * 
 * \author Matthijs
 */
void startAlarmThread(void)
{
    NutThreadSetPriority(1);
    NutThreadCreate("AlarmThread1", AlarmThread, NULL, 1024);
}

void setWeekendTime(int hour, int minute, int second)
{
    weekendtime.hour = hour;
    weekendtime.minute = minute;
    weekendtime.second = second;
}

void setAlarmATime(int hour, int minute, int second)
{
    alarmA.tm_hour = hour;
    alarmA.tm_min = minute;
    alarmA.tm_sec = second;
}

int checkWeekend()
{
    tm time;
    X12RtcGetClock(&time);

    printf("\n checkWeekend = %d-%d-%d, day if the week %d",time.tm_year, time.tm_mon, time.tm_mday, time.tm_wday);

    if(time.tm_wday == 0)
    {
        printf("X12RTC_DW: %d day of the week: %s \n", time.tm_wday, "Sunday");
        //setAlarmA(weekendtime.hour, weekendtime.minute, weekendtime.second);
        return 3;
    }
    else if(time.tm_wday == 1)
    {
        printf("X12RTC_DW: %d day of the week: %s \n", time.tm_wday, "Monday");
        //setAlarmA(weekendtime.hour, weekendtime.minute, weekendtime.second);

        return 0;
    }
    else if(time.tm_wday == 2)
    {
        printf("X12RTC_DW: %d day of the week: %s \n", time.tm_wday, "Teusday");
        //setAlarmA(weekendtime.hour, weekendtime.minute, weekendtime.second);

        return 0;
    }
    else if(time.tm_wday == 3)
    {
        printf("X12RTC_DW: %d day of the week: %s \n", time.tm_wday, "Wednesday");
       // setAlarmA(weekendtime.hour, weekendtime.minute, weekendtime.second);

        return 0;
    }
    else if(time.tm_wday == 4)
    {
        printf("X12RTC_DW: %d day of the week: %s \n", time.tm_wday, "Thursday");
       // setAlarmA(weekendtime.hour, weekendtime.minute, weekendtime.second);

        return 0;
    }
    else if(time.tm_wday == 5)
    {
        printf("X12RTC_DW: %d day of the week: %s \n", time.tm_wday, "Friday");
        //setAlarmA(weekendtime.hour, weekendtime.minute, weekendtime.second);

        return 1;
    }
    else if(time.tm_wday == 6)
    {
        printf("X12RTC_DW: %d day of the week: %s \n", time.tm_wday, "Saturday");
        //setAlarmA(weekendtime.hour, weekendtime.minute, weekendtime.second);
        return 2;
    }
    else
    {
        return 0;
    }

}


