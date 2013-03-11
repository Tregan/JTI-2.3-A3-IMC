#ifndef SHOUTCAST_INC
#define SHOUTCAST_INC

int initInet(void);
int connectToStream(void);
void setStream(char*, int);
char* getStreamInfo(void);
void playStream(void);
void stopStream(void);


#endif
