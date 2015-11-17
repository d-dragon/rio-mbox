#include "gpio.h"

void initializeGPIO(){

	wiringPiSetupGpio();
	pinMode(RELAYPIN1, OUTPUT);
	pinMode(RELAYPIN2, OUTPUT);
}

void setBinaryRelay(int pin, int value){
	digitalWrite(pin,value);
}

int getBinaryRelay(int pin){
	return digitalRead(pin);
}
