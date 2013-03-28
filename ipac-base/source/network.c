/*
 * network.c
 *
 *  Created on: 18 feb. 2013
 *      Author: Niels Ebbelink, Bas Rops
 */

#include <sys/timer.h>
#include <pro/sntp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/confnet.h>
#include <pro/dhcp.h>
#include <sys/socket.h>
#include <dev/nicrtl.h>

#include "rtc.h"
#include "display.h"
#include "player.h"
#include "log.h"
#include "vs10xx.h"
#include "menu.h"

#define ETH0_BASE	0xC300
#define ETH0_IRQ	5
#define LOG_MODULE      LOG_STREAMER_MODULE
#define OK              1
#define NOK		0

static char eth0IfName[9] = "eth0";
FILE* stream;
TCPSOCKET *sock;

int NetworkInit(void)
{
    uint8_t mac_addr[6] = { 0x00, 0x06, 0x98, 0x30, 0x02, 0x76 };

    int result = OK;

    LcdBackLight(LCD_BACKLIGHT_ON);
    LcdWriteSecondLine("Registering device...");
    printf("\nRegistering device...");

    // Registreer NIC device (located in nictrl.h)
    if(NutRegisterDevice(&DEV_ETHER, ETH0_BASE, ETH0_IRQ))
    {
        LogMsg_P(LOG_ERR, PSTR("Error: >> NutRegisterDevice()"));
        result = NOK;
    }

    if(result == OK)
    {
        LcdClearLine();
        LcdWriteSecondLine("Configuring network...");
        printf("\nConfiguring network...");

        if(NutDhcpIfConfig(eth0IfName, mac_addr, 30000))
        {
            LogMsg_P(LOG_ERR, PSTR("Error: >> NutDhcpIfConfig()"));
            result = NOK;
        }
    }

    if(result == OK)
    {
        LogMsg_P(LOG_INFO, PSTR("Networking setup OK, new settings are:\n") );	

        LogMsg_P(LOG_INFO, PSTR("if_name: %s"), confnet.cd_name);	
        LogMsg_P(LOG_INFO, PSTR("ip-addr: %s"), inet_ntoa(confnet.cdn_ip_addr) );
        LogMsg_P(LOG_INFO, PSTR("ip-mask: %s"), inet_ntoa(confnet.cdn_ip_mask) );
        LogMsg_P(LOG_INFO, PSTR("gw     : %s"), inet_ntoa(confnet.cdn_gateway) );
    }
    else
    {
        LogMsg_P(LOG_INFO, PSTR("Error: Cannot configure network.\n"));
        LogMsg_P(LOG_INFO, PSTR("Error code: %d"), NutDhcpError(eth0IfName));
    }

    NutSleep(1000);

    return result;
}

int NTP(tm* datetime)
{
    time_t ntp_time;
    uint32_t timeserver = 0;
 
    //Retrieve time from the NTP server.
    puts("\nRetrieving time from ntp");
    LcdWriteSecondLine("Retrieving time from NTP...");
 
    timeserver = inet_addr("78.192.65.63");
    
    int success = 0;
    int error = NutDhcpError(eth0IfName);
    printf("\nStatus in NTP: %d. Error: %d", NutDhcpStatus(eth0IfName), error);
    
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

int connectToStream(void)
{
    int result = NOK;
    char *data;

    sock = NutTcpCreateSocket();

    if(NutTcpConnect(sock, inet_addr("81.173.3.132"), 8082) != 0)
        LogMsg_P(LOG_ERR, PSTR("Stream not available. Errorcode: %d"), NutTcpError(sock));
    else
    {
        LogMsg_P(LOG_ERR, PSTR("Connected to stream.\n"));
        stream = _fdopen((int) sock, "r+b");

        fprintf(stream, "GET %s HTTP/1.0\r\n", "/");
        fprintf(stream, "Host: %s\r\n", "81.173.3.132");
        fprintf(stream, "User-Agent: Ethernut\r\n");
        fprintf(stream, "Accept: */*\r\n");
        fprintf(stream, "Icy-MetaData: 1\r\n");
        fprintf(stream, "Connection: close\r\n\r\n");
        fflush(stream);

        //Increase buffer size
        u_short tcpbufsiz = 8760;
        NutTcpSetSockOpt(sock, SO_RCVBUF, &tcpbufsiz, sizeof(tcpbufsiz));
        //Set read timeout of 3 seconds. After that, the stream will read 0
        u_long rx_to = 3000;
        NutTcpSetSockOpt(sock, SO_RCVTIMEO, &rx_to, sizeof(rx_to));

        // Server stuurt nu HTTP header terug, catch in buffer
        data = (char *) malloc(512 * sizeof(char));

        while(fgets(data, 512, stream))
        {
            if( 0 == *data )
                break;

            printf("%s", data);
        }

        free(data);
        result = OK;
    }

    return result;
}

void playStream(void)
{
    //Exit the menu
    menuExit();
    if(VsGetStatus() != VS_STATUS_RUNNING)
    {
        //Connect to the stream
        if(connectToStream() == OK)
            //Start playing
            play(stream);
        else
            LogMsg_P(LOG_ERR, PSTR("Stream did not start playing, error connecting."));
    }
	
    return;
}

void stopStream(void)
{
    //Exit the menu
    menuExit();
    //Stop the VsPlayer
    VsPlayerStop();
    //Tell the thread that he needs to exit
    setCloseStream(1);
    //Make sure the thread is gone, so wait half a second
    NutSleep(500);
    //Close the stream
    fclose(stream);	
	
    return;
}
