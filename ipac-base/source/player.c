#define LOG_MODULE  LOG_PLAYER_MODULE

#include <sys/heap.h>
#include <sys/bankmem.h>
#include <sys/thread.h>
#include <sys/timer.h>

#include "player.h"
#include "vs10xx.h"
#include "log.h"
#include "network.h"
#include "menu.h"

#define OK			1
#define NOK			0

int closeStream;
int fallingAsleepMode;
int fallingAsleepTime;

THREAD(FallingAsleep, arg)
{
    printf("falling asleep started\n");
    for(;;)
    {
        int volumeGoal = 254;
        
        //Dont start yet if no sound is playing
        while(VsGetStatus() != VS_STATUS_RUNNING)
            NutSleep(50);

        //Get the amount of seconds from the fallingAsleepTime minutes
        long totalSeconds = (long)fallingAsleepTime * 1000 * 60;
        printf("totalseconds = %lu\n", totalSeconds);
        //Get the time to sleep before "increasing" the volume. Divide the totalSeconds by the volumeGoal to get a smooth transition
        long sleepTime = totalSeconds / volumeGoal;
        printf("sleeptime = %lu\n", sleepTime);
        int currentVolume = VsGetVolume();
        
        //"Increase" the volume until it hits the goal
        while(currentVolume < volumeGoal)
        {
            NutSleep(sleepTime);
            VsSetVolume(currentVolume + 1, currentVolume + 1);
            currentVolume++;
        }
        
        //Call stopStream in network.c
        stopStream();
        //Turn off falling asleep mode
        fallingAsleepMode = 0;
        //Reset volume
        VsSetVolume(0, 0);
        //Exit the thread
        NutThreadExit();
    }
}

THREAD(StreamPlayer, arg)
{
    FILE *stream = (FILE *) arg;
    size_t rbytes = 0;
    char *mp3buf;
    int result = NOK;
    int nrBytesRead = 0;
    unsigned char iflag;
    
    //
    // Init MP3 buffer. NutSegBuf is een globale, systeem buffer
    //
    if(NutSegBufInit(8192) != 0)
    {
        // Reset global buffer
        iflag = VsPlayerInterrupts(0);
        NutSegBufReset();
        VsPlayerInterrupts(iflag);

        result = OK;
    }
    
    // Init the Vs1003b hardware
    if(result == OK)
    {
        if(VsPlayerInit() == -1)
        {
            if(VsPlayerReset(0) == -1)
            {
                result = NOK;
            }
        }
    }

    for(;;)
    {
        /*
         * Query number of byte available in MP3 buffer.
         */
        iflag = VsPlayerInterrupts(0);
        mp3buf = NutSegBufWriteRequest(&rbytes);
        VsPlayerInterrupts(iflag);

        // Bij de eerste keer: als player niet draait maak player wakker (kickit)
        if(VsGetStatus() != VS_STATUS_RUNNING)
        {
            VsPlayerKick();
            //Why would you wait to start the player...?
            /*if(rbytes < 1024)
            {
                printf("VsPlayerKick()\n");
                VsPlayerKick();
            }*/
        }

        while(rbytes && !closeStream)
        {
            // Copy rbytes (van 1 byte) van stream naar mp3buf.
            nrBytesRead = fread(mp3buf,1,rbytes,stream);

            if(nrBytesRead > 0)
            {
                iflag = VsPlayerInterrupts(0);
                mp3buf = NutSegBufWriteCommit(nrBytesRead);
                VsPlayerInterrupts(iflag);
                if(nrBytesRead < rbytes && nrBytesRead < 512)
                {
                    NutSleep(250);
                }
            }
            else
            {
                break;
            }
            rbytes -= nrBytesRead;

            if(nrBytesRead <= 0)
            {
                break;
            }				
        }
        
        if(closeStream)
        {
            closeStream = 0;
            NutThreadExit();
        }
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Initialize the player
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
int initPlayer(void)
{
    closeStream = 0;
    fallingAsleepMode = 0;
    return OK;
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief set the value of closeStream
 * \parameter value the new value of closeStream. 0 = keep stream on, 1 = close stream
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
void setCloseStream(int value)
{
    closeStream = value;
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief set the value of fallingAsleepMode
 * \parameter value the new value of fallingAsleepMode. 0 = no falling asleep mode, 1 = falling asleep mode enabled
 * \parameter time the time that the music needs to go on, in minutes
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
void enableFallingAsleepMode(void)
{
    menuExit();
    if(time > 0)
    {
        //Set variable of mode to 1, falling asleep mode enabled
        fallingAsleepMode = 1;
        //Set falling asleep time to 1 hour
        fallingAsleepTime = 60;
        //Create the thread for falling asleep
        NutThreadCreate("FallingAsleep", FallingAsleep, NULL, 1024);
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief set the value of fallingAsleepMode
 * \return The value of fallingAsleepMode
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
int getFallingAsleepMode(void)
{
    return fallingAsleepMode;
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief play music from a stream
 * \parameter stream a pointer to a (File) stream
 * \author Bas
 */
/* ����������������������������������������������������������������������� */
int play(FILE *stream)
{
    NutThreadCreate("Bg", StreamPlayer, stream, 512);
    printf("\nPlay thread created. Device is playing stream now !\n");

    return OK;
}
