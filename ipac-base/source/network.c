/*
 * network.c
 *
 *  Created on: 18 feb. 2013
 *      Author: Niels Ebbelink
 */

//in boards there are two lines to be commented out
#include <dev/board.h>
#include <sys/timer.h>
#include <sys/confnet.h>

#include <network.h>

#include <pro/dhcp.h>
#include <pro/sntp.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <arpa/inet.h>

#include <net/route.h>
#include <string.h>
#include <time.h>
#include "rtc.h"
#include "display.h"
 

//Hard coded network configuration.
//#define MY_MAC  { 0x00, 0x06, 0x98, 0x30, 0x02, 0x76 }
//#define MY_IP   "145.48.224.228"
//#define MY_MASK "255.255.254.0"

int NetworkInit()
{
    //Register Ethernet controller.
    LcdBackLight(LCD_BACKLIGHT_ON);
    LcdWriteSecondLine("Registering device...");
    printf("\nRegistering device...");
    if (NutRegisterDevice(&DEV_ETHER, 0, 0)) 
    {
        puts("Registering " DEV_ETHER_NAME " failed.");
    }
    else
    {
        LcdClearLine();
        LcdWriteSecondLine("Configuring network...");
        printf("\nConfiguring network...");
        
        //Configure network.
        if (NutDhcpIfConfig(DEV_ETHER_NAME, NULL, 60000) == 0)
            printf("\nNetwork configured. Now try 'ping %s' on your PC.\n", inet_ntoa(confnet.cdn_ip_addr));
        else
        {
            puts("\nError: Cannot configure network.");
            printf("%d", NutDhcpError(DEV_ETHER_NAME));
        } 
    }
    
    LcdClearLine();
    LcdBackLight(LCD_BACKLIGHT_OFF);
    return 0;
}

int NTP(tm* datetime)
{
    time_t ntp_time = 2;
    uint32_t timeserver = 0;
 
    //Retrieve time from the NTP server.
    puts("\nRetrieving time from ntp");
    LcdWriteSecondLine("Retrieving time from NTP...");
 
    timeserver = inet_addr("78.192.65.63");
    
    int success = 0;
    int error = NutDhcpError(DEV_ETHER_NAME);
    printf("\nStatus in NTP: %d. Error: %d", NutDhcpStatus(DEV_ETHER_NAME), error);
    
    if(error == 0)
    {
        int attemptNr = 0;
        for (;;) 
        {
            if (NutSNTPGetTime(&timeserver, &ntp_time) == 0) 
            {
                success = 1;
                break;
            }
            else 
            {
                NutSleep(1000);
                printf("\n%d, Failed to retrieve time. Retrying...", attemptNr);

                if(attemptNr == 10)
                {
                    break;
                }
                attemptNr++;
            }
        }
    }
    
    //If time was not retrieved from the NTP server
    if(!success)
    {
        printf("\nError retrieving NTP. Time not synced with the NTP server!");
        datetime->tm_hour = 0;
        datetime->tm_min = 0;
        datetime->tm_sec = 0;

        datetime = localtime(&ntp_time);
        X12RtcSetClock(datetime);

        LcdClearLine();
        LcdBackLight(LCD_BACKLIGHT_OFF);

        return 0;
    }
 
    datetime = localtime(&ntp_time);
    X12RtcSetClock(datetime);
 
    printf("\nSuccess. NTP time is: %02d:%02d:%02d %d-%d-%d", datetime->tm_hour, datetime->tm_min, datetime->tm_sec, datetime->tm_mday, datetime->tm_mon + 1, datetime->tm_year + 1900);
    LcdClearLine();
    
    return 1;
}
