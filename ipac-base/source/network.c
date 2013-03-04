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
 

//Hard coded network configuration.
//#define MY_MAC  { 0x00, 0x06, 0x98, 0x30, 0x02, 0x76 }
//#define MY_IP   "145.48.224.228"
//#define MY_MASK "255.255.254.0"

int NetworkInit()
{
    printf("in networkinit\n");
 
    //Register Ethernet controller.
    //TODO test if the if-statement can go away, it shouldn't have to be here.
//    if (NutRegisterDevice(&DEV_ETHER, 0, 0)) 
//    {
//        puts("Registering " DEV_ETHER_NAME " failed.");
//    }
    //Configure network.
    //TODO test the timeout, 10 seconds atm, but may need to be higher or lower.
    if (NutDhcpIfConfig(DEV_ETHER_NAME, NULL, 10000)) 
    {
        puts("Configuring " DEV_ETHER_NAME " failed.");
    }
    else {
        printf("Now try 'ping %s' on your PC.\n", inet_ntoa(confnet.cdn_ip_addr));
    }

    return 0;
}

int NTP(tm* datetime)
{
    time_t ntp_time = 2;
    uint32_t timeserver = 0;
 
    puts("NTP\n");
 
    //Retrieve time from the NTP server.
    puts("Retrieving time from ntp");
 
    timeserver = inet_addr("78.192.65.63");
    
    int attemptNr = 0;
    for (;;) 
    {
        if (NutSNTPGetTime(&timeserver, &ntp_time) == 0) 
        {
            break;
        } else 
        {
            NutSleep(1000);
            printf("%d, Failed to retrieve time. Retrying...\n", attemptNr);
            
            if(attemptNr == 10)
            {
                printf("Retrieving NTP, server timed out. There will not be synced with the NTP server!\n");
                datetime->tm_hour = 0;
                datetime->tm_min = 0;
                datetime->tm_sec = 0;
                return 0;
            }
            attemptNr++;
        }
    }
    puts("Done.\n");
 
    datetime = localtime(&ntp_time);
    X12RtcSetClock(datetime);
 
    printf("NTP time is: %02d:%02d:%02d\n", datetime->tm_hour, datetime->tm_min, datetime->tm_sec);
    
    return 1;
}


