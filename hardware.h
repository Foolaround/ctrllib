#ifndef HARDWARE_H
#define HARDWARE_H 1

#include "WProgram.h"
#include "ctrllib.h"

/* Definitionen der Hardware - Typen */
extern hw_def DIGITAL_IN, DIGITAL_OUT, ANALOG_IN, ANALOG_OUT, PCF8591_IN, PCF8574_OUT, ENCODER_IN, SERVO;

/* Definitionen der Wrapper - Funktionen */
void init_display();
void msg(char *, char*);
void bar(byte val);
void val(int val, char *unit);

int eepromRead(byte address);
void eepromWrite(byte address, byte val);

int serialAvailable();
char serialRead();
void serialWrite(char *);


#endif
