/* 
 * File:   main.h
 * Author: Bas
 *
 * Created on 4 maart 2013, 13:46
 */

#ifndef MAIN_H
#define	MAIN_H

#define BACKLIGHT_OFF 0
#define BACKLIGHT_ON 1

#define DATETIME_HOURS 0
#define DATETIME_MINUTES 1
#define DATETIME_SECONDS 2

#define DATETIME_DAYS 0
#define DATETIME_MONTHS 1
#define DATETIME_YEARS 2

#ifdef	__cplusplus
extern "C" {
#endif
tm SchedulerDate1;
tm SchedulerDate2;
void SchedulerDate(void);     
void SetTimezone(void);
void SyncDatetime(void);
void SetTimeManually(void);
void SetDateManually(void);
void ResetBacklightCounter(void);
void SetBacklightStayOn(int);
void AlarmAMenu(void);
void AlarmBMenu(void);
void AlarmWeekendMenu(void);
void MainAlarmBMenu(void);
void NoteAlarmBMenu(void);
void SchedulerMenu(void);
void VolumeMenu(void);

#ifdef	__cplusplus
}
#endif

#endif	/* MAIN_H */

