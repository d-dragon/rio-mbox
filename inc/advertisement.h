/*
 * advertisement.h
 *
 *  Created on: Aug 5, 2014
 *      Author: duyphan
 */

#ifndef ADVERTISEMENT_H_
#define ADVERTISEMENT_H_


#define COUNT_MAX 10
#define MESSAGE_INVALID 0
#define MESSAGE_VALID 1

char *send_recv_buff;


void *advertiseServerInfoThread();
char *AdvPackageWrapper(char * ,char *);
int parseDiscoveredMessage(char *);

void startMulticastListener(char *mac_addr);

#endif /* ADVERTISEMENT_H_ */
