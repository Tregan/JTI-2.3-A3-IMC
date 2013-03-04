
///
#define LOG_MODULE  LOG_PLAYER_MODULE

#include <sys/heap.h>
#include <sys/bankmem.h>
#include <sys/thread.h>
#include <sys/timer.h>

#include "player.h"
#include "vs10xx.h"
#include "log.h"



#define OK			1
#define NOK			0

THREAD(StreamPlayer, arg);

int initPlayer(void)
{
	return OK;
}

int play(FILE *stream)
{
	NutThreadCreate("Bg", StreamPlayer, stream, 512);
	printf("Play thread created. Device is playing stream now !\n");

	
	return OK;
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
	if( 0 != NutSegBufInit(8192) )
	{
		// Reset global buffer
		iflag = VsPlayerInterrupts(0);
		NutSegBufReset();
		VsPlayerInterrupts(iflag);
		
		result = OK;
	}
	
	// Init the Vs1003b hardware
	if( OK == result )
	{
		if( -1 == VsPlayerInit() )
		{
			if( -1 == VsPlayerReset(0) )
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
		if( VS_STATUS_RUNNING != VsGetStatus() )
		{
			if( rbytes < 1024 )
			{
				printf("VsPlayerKick()\n");
				VsPlayerKick();
			}
		}
		
		while( rbytes )
		{
			// Copy rbytes (van 1 byte) van stream naar mp3buf.
			nrBytesRead = fread(mp3buf,1,rbytes,stream);
			
			if( nrBytesRead > 0 )
			{
				iflag = VsPlayerInterrupts(0);
				mp3buf = NutSegBufWriteCommit(nrBytesRead);
				VsPlayerInterrupts(iflag);
				if( nrBytesRead < rbytes && nrBytesRead < 512 )
				{
					NutSleep(250);
				}
			}
			else
			{
				break;
			}
			rbytes -= nrBytesRead;
			
			if( nrBytesRead <= 0 )
			{
				break;
			}				
		}
	}
}

