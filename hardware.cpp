#include "hardware.h"
#include <Wire.h>
#include <EEPROM.h>;

// #define NOLCD 1 // Zum debuggen

/* Auswertung des Rückgabecodes des I2C Busses */
void checkWire(byte ret) {
  if(!ret)
    return;
  char *error = "?";
  if(ret == 1) error = "data too long";
  if(ret == 2) error = "NACK on address";
  if(ret == 3) error = "NACK on data";
  if(ret == 4) error = "other";
  Serial.print("Error on I2C:");
  Serial.println(error);
}

/* Ansteuerung des Servo-Motors über serielle Schnittstelle */
void createSerial(int adr, int mode) {
  Serial1.begin(mode);
}
void serialWrite(int adr, int val) {
  Serial1.print(0x80);
  Serial1.print(0x00);
  Serial1.print((val >> 8));
  Serial1.print((uint8_t) (val & 0xFF));
}

/* Graycode dekodieren */
#define SHIFTCHEAT 20 // Offset um alle Encoderadresse doppelt in 64 bit unterbringen zu können
void createEncoder(int adr, int mode) {
  pinMode(adr & 0xFF, mode);
  digitalWrite(adr & 0xFF, HIGH);
  pinMode(adr >> 8, mode);
  digitalWrite(adr >> 8, mode);
}
uint64_t oe;
int EncoderRead(int adr) {
   int8_t enc_states [] = { 0, - 1, 1, 0, 1, 0, 0, - 1, - 1, 0, 0, 1, 0, 1, - 1, 0 };
   uint8_t a = adr & 0xFF;
   uint8_t b = adr >> 8;
   
   bitWrite(oe, 32+a-SHIFTCHEAT, bitRead(oe, a-SHIFTCHEAT));
   bitWrite(oe, 32+b-SHIFTCHEAT, bitRead(oe, b-SHIFTCHEAT));
   bitWrite(oe,    a-SHIFTCHEAT, digitalRead(a));
   bitWrite(oe,    b-SHIFTCHEAT, digitalRead(b));
   
   return enc_states[ bitRead(oe, a+32-SHIFTCHEAT) << 3 | bitRead(oe, b+32-SHIFTCHEAT) << 2 | bitRead(oe, a-SHIFTCHEAT) << 1 | bitRead(oe, b-SHIFTCHEAT) ];
}

/* Digitale und analoge Pins */
void setPinMode(int adr, int mode) {
  pinMode((byte) (adr & 0xFF), (byte) mode);
  digitalWrite((byte)(adr & 0xFF), (mode == INPUT) ? HIGH : LOW);
}
void digWrite(int adr, int val) {
  digitalWrite((byte)(adr & 0xFF), (byte)(val & 0xFF));
}
int digRead(int adr) {
  return (int) digitalRead((byte)(adr & 0xFF));
}
void anWrite(int adr, int val) {
  analogWrite((byte)(adr & 0xFF), (byte)(val & 0xFF));
}
int anRead(int adr) {
  return (int) analogRead((byte)(adr & 0xFF));
}

/* PCF Bausteine */
int PCF8591Read(int adr) {
  int ret;
  Wire.beginTransmission((uint8_t)(adr >> 8));
  Wire.send((uint8_t)(adr & 0xFF));
  checkWire(Wire.endTransmission());

  Wire.requestFrom((uint8_t)(adr >> 8), (uint8_t) 2);
  while(Wire.available()) {
    ret = Wire.receive();
  }
  return ret;
}
void PCF8574Write(int adr, int val) {
  int ret;
  Wire.beginTransmission((uint8_t)(adr >> 8));
  Wire.send((uint8_t)(val & 0xFF));
  checkWire(Wire.endTransmission());
}

/* Display */
#define DISPLAYI2C 0x3B
bool lastrow = 1;

void init_display() {
  byte ret;
  delay(2);
#ifdef NOLCD
  return;
#endif
  Wire.beginTransmission(DISPLAYI2C);
  Wire.send(0x00);  // control byte for instruction
  Wire.send(0x34);  // 2*16 Display
  Wire.send(0x0C);  // no cursor
  Wire.send(0x06);  // auto inc cursor

 
  Wire.send(0x35);  // extended instruction set
  Wire.send(0x04);  // left to right, top to bottom
  Wire.send(0x10);  // TC1: 0, TC2: 0
  Wire.send(0x42);  // HV Stages 3
  Wire.send(0x9f);  // set Vlcd, store VA
  Wire.send(0x34);  // normal instructin set
  Wire.send(0x80);  // reset DDRAM to 0x0
  Wire.send(0x01);  // return home
  
  checkWire(Wire.endTransmission());
}

void updateLine(char *line, byte row) {
#ifdef NOLCD
  return;
#endif
  byte i;
 /* Position */
  Wire.beginTransmission(DISPLAYI2C);
  Wire.send(0x00);
  Wire.send((row == 1) ? 0x80: 0xC0);
  checkWire(Wire.endTransmission());
  
  /* Write line */
  Wire.beginTransmission(DISPLAYI2C);
  Wire.send(0x40);  // Control byte for data  
  for(i = 0; line[i] && i < 16; i++)
    Wire.send(line[i] | 0x80);
  for(; i < 16; i++)
    Wire.send(0xA0);
  checkWire(Wire.endTransmission());
}

void msg(char *line1, char *line2) {
  static char l1[21] = "x";
  static char l2[21] = "x";
  
  if(line1 && strcmp(l1, line1)) {
    strcpy(l1, line1);
    Serial.println(line1);
    updateLine(line1, 1);
  }
  if(line2 && strcmp(l2, line2)) {
    strcpy(l2, line2);
    lastrow = 1;
    Serial.println(line2);
    updateLine(line2, 2);
  }
}

void val(int val, char *unit) {
  Wire.beginTransmission(DISPLAYI2C);
  Wire.send(0x00);
  Wire.send(0xC0);
  checkWire(Wire.endTransmission());

  /* Write line */
  Wire.beginTransmission(DISPLAYI2C);
  Wire.send(0x40);  // Control byte for data
  byte i = 0;
  byte j = 0;
  int d = 10000;
  for(i = 0; i < 16; i++) {
    if(val < 0) {
      Wire.send('-');
      j++;
    } else if(d && val / d > 1) {
      Wire.send((val / d) + '0');
      d /= 10;
      j++;
    } else if(unit && unit[i-j]) {
      Wire.send(unit[i-j]);
    } else
      Wire.send(' ');
  }
  checkWire(Wire.endTransmission());
}

void bar(byte val) {
  static byte oldval = 0;
  
  if(val == oldval && !lastrow)
    return;
  lastrow = 0;
  oldval = val;
#ifdef NOLCD
  return;
#endif
  byte i;
  float p;
  
  p = ((float) val/255);
  
  Wire.beginTransmission(DISPLAYI2C);
  Wire.send(0x00);
  Wire.send(0xC0);
  checkWire(Wire.endTransmission());

  /* Write line */
  Wire.beginTransmission(DISPLAYI2C);
  Wire.send(0x40);  // Control byte for data  
  for(i = 0; i < 16; i++) {    
    if(((float) i/16.0) < p)
      Wire.send(0x13);      
    else if(((float) i/16.0)-0.010416 < p)
      Wire.send(0x7E);
     else if(((float) i/16.0)-0.020832 < p)
      Wire.send(0x7D); 
    else if(((float) i/16.0)-0.031248 < p)
      Wire.send(0x7C);      
    else if(((float) i/16.0)-0.041664 < p)
      Wire.send(0x7B);   
    else
      Wire.send(0xA0);
  }
  checkWire(Wire.endTransmission());
}

/* EEPROM */
int eepromRead(byte address) {
  return EEPROM.read(address);
}
void eepromWrite(byte address, byte val) {
  EEPROM.write(address, val);
}

/* Serielle Schnittstelle */
int serialAvailable() {
  return Serial.available();
}
char serialRead() {
  return Serial.read();
}
void serialWrite(char *v) {
  Serial.println(v);
}


/* Hardware - Typen */

hw_def DIGITAL_IN  = { INPUT, setPinMode, NULL, digRead };
hw_def DIGITAL_OUT = { OUTPUT, setPinMode, digWrite, NULL };
hw_def ANALOG_IN   = { 0, NULL, NULL, anRead };
hw_def ANALOG_OUT  = { OUTPUT, setPinMode, anWrite, NULL };
hw_def PCF8591_IN  = { 0, NULL, NULL, PCF8591Read };
hw_def PCF8574_OUT = { 0, NULL, PCF8574Write, NULL };
hw_def ENCODER_IN  = { INPUT, createEncoder, NULL, EncoderRead };
hw_def SERVO       = { 9600, createSerial, serialWrite, NULL };
