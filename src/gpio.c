#include "gpio.h"

void initializeGPIO(){

	wiringPiSetupGpio();
	pinMode(RELAYPIN1, OUTPUT);
	pinMode(RELAYPIN2, OUTPUT);
}

void setBinary(int pin, int value){
	digitalWrite(pin,value);
}

int getBinary(int pin){
	return digitalRead(pin);
}
