/*
 * network.h
 *
 *  Created on: 18 feb. 2013
 *      Author: Niels Ebbelink
 */
#include "time.h"
#ifndef NETWORK_H_
#define NETWORK_H_

int NetworkInit(void);
int NTP(tm*);
//int connectToStream(void);
void playStream(void);
void stopStream(void);

#endif /* NETWORK_H_ */
