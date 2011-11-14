#ifndef CTRLLIB_H
#define CTRLLIB_H 1

#include "WProgram.h"

/* Definitionen für "sprechendere" Werte */
#define ON 1
#define OPEN 1
#define PRESSED 1
#define OFF 0
#define CLOSED 0
#define NOT_PRESSED 0

/* Hilfstypen für Objekte */
#define TYPE_HW     0b0001
#define TYPE_PARAM  0b0010
#define TYPE_STATE  0b0100

/* Standardpause für Menüfunktionen */
#define DELAY_MENU 256


/* Hardwaredefinitionen */
typedef struct hw_def {
  byte mode;  // Input/Outpuut
  void (*init)(int, int); // Funktion zur initialisierung
  void (*setVal)(int, int); // Funktion zum Setzen eines Ausgangs
  int (*getVal)(int); // Funktion zum Lesen eines Eingangs
} hw_def;

/* Objekt - Klasse */
class Object
{
protected:
  int val;
  char name[8];
  Object *next;
public:
  Object & operator=(int val);
  Object & operator=(char *val);
  Object & operator=(Object val);
  bool operator==(int val);
  bool operator==(Object val);
  bool operator!=(int val);
  bool operator!=(Object val);
  bool operator<=(int val);
  bool operator<=(Object val);
  bool operator>=(int val);
  bool operator>=(Object val);
  bool operator<(int val);
  bool operator<(Object val);
  bool operator>(int val);
  bool operator>(Object val);
  Object *getNext();
  byte otype;
  Object(byte otype, char *name);
  char *getName();
  int getValue();
  void setValue(int);
  void setValueByName(char *);
  void showValue();
};

/* Struktur für registrierte Zustandsfunktionen */
typedef void (* state)();
typedef struct {
  char name[8];
  state s; // Zustandsfunktion
  state i; // Initialisierungsfunktion für Zustand
} sstate;

class Hardware;
/* Zustands - Klasse */
class State : public Object
{
private:
  sstate *states;
  int maxstates;
  bool init;
  long start;
public:
  using Object::operator=;
//  int buffer[BUFFERSIZE];
  long timer;
  bool error;
  void setValue(int val);
  void setValueByName(char *val);
  int getValue();
  int execute();
  void menu(Hardware);
  void menu_enter(Hardware enc, Hardware b, State *s);
  uint8_t menu_exit(Hardware b, State *s);
  State(char *name);
  void register_state(char *name, state s, state i);
};

/* Hardware - Klasse */
class Hardware : public Object
{
private:
  int address;
  hw_def hw;
  int (*convertFunc)(int);
  long timer;
public:
  using Object::operator=;
  int *buffer;
  Hardware(char *_name, int _address, hw_def hw, int (*_convertFunc)(int), int *b);
  /* how long has this device been "on"? */
  int getTime();
  void setValue(int val);
  void setValueByName(char *val);
  int getValue();
  void showValue();
};

/* Parameter - Klasse */
class Param : public Object
{
private:
  int address;
public:
  using Object::operator=;
  Param(char *_name, int _address, int _default);
  void setValue(int val);
  void setValueByName(char *val);
  int getValue();
};

/* Wrapper - Klassen, müssen architekturabhängig implementiert werden */
extern void msg(char *, char*);
extern int eepromRead(byte address);
extern void eepromWrite(byte address, byte val);

extern int serialAvailable();
extern char serialRead();
extern void serialWrite(char *);

/* Handling serielle Kommunikation */
void serial();

#endif

