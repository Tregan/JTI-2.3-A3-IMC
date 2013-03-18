#ifndef PLAYER_INC
#define PLAYER_INC

#include <sys/nutconfig.h>
#include <sys/types.h>

//#include <stdlib.h>
//#include <string.h>
#include <stdio.h>
#include <io.h>

int initPlayer(void);
void setCloseStream(int);
void enableFallingAsleepMode(void);
int getFallingAsleepMode(void);
int play(FILE *stream);


#endif
