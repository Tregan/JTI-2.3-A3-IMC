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

/*!
 * \brief thread that checks the alarms every 10 seconds
 * 
 * \author Matthijs
 */
THREAD(AlarmThread, args)
{
    u_long flags;
    alarmBStruct newSet;
    for(;;)
    {
        int succes = X12RtcGetStatus(&flags);
        int i;
        
        for(i = 0; i <=10; i++)
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
            printf("\n ========== Alarm 0 =========== \n");       
            startSnoozeThreadA();				
            X12RtcClearStatus(32);
        }

        //alarm B
        if(flags == 64)
        {
            printf("\n ============ Alarm 1================== \n");
           
            int k;
            for(k = 0; k <= 10; k++)
            {
                if(alarmBArray[k].index == currentAlarm.index)
                {
                    alarmBArray[k].set = 0;
                }
            }
            startSnoozeThreadB();
            X12RtcClearStatus(64);
            newSet = checkFirst();
            X12RtcSetAlarm(1, &newSet.timeSet, 31);
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

/*
 * \brief       compares two time structs, 
 *              returns 0 if current is bigger after end
 *              returns 1 if current is smaller then end
 *              return 2 if they are the same
 */
int compareTime(tm current, tm end)
{
    if(current.tm_year > end.tm_year)
    {
        //alarm uit zettern
        return 0;
    }
    else if(current.tm_year ==  end.tm_year)
    {
        if(current.tm_yday >  end.tm_yday)
        {  
            //alarm uit zettern
            return 0;
        }
        else if(current.tm_yday ==  end.tm_yday)
        {
            if(current.tm_hour > end.tm_hour)
            {
                //alarm uit zettern
                return 0;
            }
            else if(current.tm_hour == end.tm_hour)
            {
                if(current.tm_min > end.tm_min)
                {
                    //alarm uit zettern
                    return 0;
                }
                else if(current.tm_min == end.tm_min)
                {
                    if(current.tm_sec > end.tm_sec)
                    {
                        //alarm uit zettern
                        return 0;
                    }
                    else if(current.tm_sec > end.tm_sec)
                    {
                        return 2;
                    }
                }
            }
        }
    }
    return 1;
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
 */
void save(void)
{
    //write the alarmBArray to the sram
    At45dbPageWrite(5, &alarmBArray, sizeof(alarmBArray));
}

/*
 * \brief load the alarms from the sram
 * 
 * \author Matthijs
 */
void load(void)
{
    //read the alarmBArray from the sram
    At45dbPageRead(5,&alarmBArray, sizeof(alarmBArray));
}

/*
 * \brief setting the alarms at a initiallizer
 * 
 * \author Matthijs
 */
void createAlarms(void)
{   
    tm startTime;
    startTime.tm_year = 0;
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
    load();
    alarmBStruct first = checkFirst();
    currentAlarm = first;
    X12RtcSetAlarm(1, &first.timeSet, 31);
    
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
    int set = 0;
    alarmBStruct first;
    for(i = 0; i <= 10; i++)
    {
        if(alarmBArray[i].set == 1)
        {
             first = alarmBArray[i];
             set = 1;
             break;
        }
    }
    
    if(set == 1)
    {
        for(i = 0; i<= 10; i++)
        {
            if(alarmBArray[i].set == 1)
            {
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
                        if(first.timeSet.tm_hour > alarmBArray[i].timeSet.tm_hour)
                        {
                             first = alarmBArray[i];
                        }
                        else if(first.timeSet.tm_hour == alarmBArray[i].timeSet.tm_hour)
                        {
                            if(first.timeSet.tm_min > alarmBArray[i].timeSet.tm_min)
                            {
                                 first = alarmBArray[i];
                            }
                            else if(first.timeSet.tm_min == alarmBArray[i].timeSet.tm_min)
                            {
                                if(first.timeSet.tm_sec >= alarmBArray[i].timeSet.tm_sec)
                                {
                                     first = alarmBArray[i];
                                }
                            }
                        }
                    }
                }
            }
        }
        return first;
    }
    alarmBStruct off;
    off.timeSet.tm_year = 0;
    return off;
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
    save();
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


