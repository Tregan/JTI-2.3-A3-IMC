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

#ifdef	__cplusplus
extern "C" {
#endif
    
void setTimezone(void);
void setTimeManually(void);
void resetBacklightCounter(void);
void setBacklightStayOn(int);
    
#ifdef	__cplusplus
}
#endif

#endif	/* MAIN_H */

