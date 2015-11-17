#ifndef _GPIO_H_
#define _GPIO_H_

#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>

#define RELAYPIN1 17
#define RELAYPIN2 22

void initializeGPIO();

void setBinaryRelay(int pin, int value);
int getBinaryRelay(int pin);
#endif
