#ifndef SHOUTCAST_INC
#define SHOUTCAST_INC

int initInet(void);
int connectToStream(void);
void setStream(char*, int);
char* getStreamInfo(void);
int playStream(void);
int stopStream(void);


#endif
