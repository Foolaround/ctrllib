#include "ctrllib.h"

/* serial communication */
#define SBUF 20
char serialbuffer[SBUF] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/* build hash list of last X bits of first two chars XOR'd  */
#define OBHASHSIZE 0x0111   //3 bits
extern Object *ob[OBHASHSIZE +1];

void oom(void *p) {
  if(p)
    return;
  msg("Error:", "OOM");
  exit(1);
}

// Constructor for all objects, links them in a biiig list of pointers to objects 
Object::Object(byte _otype, char *_name) {
  memcpy(this->name, _name, 7);
  this->name[7] = 0;
  this->next = NULL;
  this->val = 0;
  this->otype = _otype;
  byte hash = (byte) (getName()[0] & OBHASHSIZE);
  hash ^= (byte) (getName()[1] & OBHASHSIZE);
  this->next = ob[hash];
  ob[hash] = this;
}

/* Operatoren */
Object & Object::operator=(int val) {
  this->setValue(val);
  return *this;
}
Object & Object::operator=(char *val) {
  this->setValueByName(val);
  return *this;
}
bool Object::operator==(int val) {
  return (this->getValue() == val);
}
bool Object::operator!=(int val) {
  return (this->getValue() != val);
}
bool Object::operator<=(int val) {
  return (this->getValue() <= val);
}
bool Object::operator>=(int val) {
  return (this->getValue() >= val);
}
bool Object::operator<(int val) {
  return (this->getValue() < val);
}
bool Object::operator>(int val) {
  return (this->getValue() > val);
}

Object & Object::operator=(Object val) {
  this->setValue(val.getValue());
  return *this;
}
bool Object::operator==(Object val) {
  return (this->getValue() == val.getValue());
}
bool Object::operator!=(Object val) {
  return (this->getValue() != val.getValue());
}
bool Object::operator<=(Object val) {
  return (this->getValue() <= val.getValue());
}
bool Object::operator>=(Object val) {
  return (this->getValue() >= val.getValue());
}
bool Object::operator<(Object val) {
  return (this->getValue() < val.getValue());
}
bool Object::operator>(Object val) {
  return (this->getValue() > val.getValue());
}

/* maps to child's implementations */
int Object::getValue() {
  switch(this->otype) {
    case TYPE_HW:    return ((Hardware *) this)->getValue(); break;
    case TYPE_PARAM: return ((Param *) this)->getValue(); break;
    case TYPE_STATE: return ((State *) this)->getValue(); break;
    default:         return this->val; break;
  }
}
void Object::setValue(int val) {
  switch(this->otype) {
    case TYPE_HW:     ((Hardware *) this)->setValue(val); break;
    case TYPE_PARAM:  ((Param *) this)->setValue(val); break;
    case TYPE_STATE:  ((State *) this)->setValue(val); break;
    default:          this->val = val; break;
  }
}
void Object::setValueByName(char *val) {
  switch(this->otype) {
    case TYPE_HW:     ((Hardware *) this)->setValueByName(val); break;
    case TYPE_PARAM:  ((Param *) this)->setValueByName(val); break;
    case TYPE_STATE:  ((State *) this)->setValueByName(val); break;
    default:          this->val = atoi(val); break;
  }
}

void Object::showValue() {
  Serial.print(this->name);
  Serial.print(": ");
  Serial.println(this->getValue());
}

char *Object::getName() {
  return this->name;
}

Object *Object::getNext() {
  return this->next;
}

Object *ob[OBHASHSIZE + 1];

void State::setValueByName(char *val) {
  int i;
  for(i = 0; i < maxstates; i++)
    if(!strcmp(states[i].name, val))
      break;
  if(i >= maxstates)
    return;
  this->setValue(i);
}

void State::setValue(int state) {
  this->init = false;
  this->val = state;
}
  
int State::getValue() {
  return this->val;
}

void State::menu(Hardware h) {
  int8_t dir = h.getValue();
  if(!dir || this->timer < DELAY_MENU) return;
  int newstate = this->val + dir;
  if(newstate >= this->maxstates) newstate = 0;
  if(newstate < 0) newstate = this->maxstates - 1;
  this->setValue(newstate);
}

void State::menu_enter(Hardware enc, Hardware b, State *s) {
  if(s->getValue() != 0)
    s->execute();
  else if(b.getValue() == ON && (this->timer > (DELAY_MENU*2)))
    s->setValue(1);
  else
    this->menu(enc);
}
uint8_t State::menu_exit(Hardware b, State *s) {
  if(b == ON && this->timer > DELAY_MENU * 2) {
    this->setValue(0);
    s->setValue(0);
    return 1;
  }
  return 0;  
}

int State::execute() {
  bool error = 0;
  if(this->init == false) {
    this->timer = 0;
    this->start = millis();
    if(this->states[val].i) this->states[val].i();
    this->init = true;
  } else {
    this->timer = millis() - this->start;
  }

  /* execute state. this might change state function pointer and error value */
  this->states[val].s();
  
  /* catch thrown errors for higher level states */
  error = this->error;
  this->error = 0;
  return error;
}

void State::register_state(char *_name, state s, state i) {
  states = (sstate *) realloc(states, sizeof(sstate) * (maxstates + 1));
  oom(states);
  memcpy(states[maxstates].name, _name, 7);
  states[maxstates].name[7] = 0;
  states[maxstates].s = s;
  states[maxstates].i = i;
  maxstates++;
}

State::State(char *_name) : Object(TYPE_STATE, _name) {
  error = 0;
  init = false;
  maxstates = 0;
  this->val = 0;
}

// XXX register timer hooks

Hardware::Hardware(char *_name, int _address, hw_def _hw,
           int (*_convertFunc)(int),
           int *_buffer) : Object(TYPE_HW, _name)
{
  memcpy(name, _name, 7); 
  name[7] = 0;
  hw = _hw;
  address = _address;
  convertFunc = _convertFunc;
  buffer = _buffer;
  val = 0;

  /* Configure hardware */
  if(this->hw.init)
    this->hw.init( address, this->hw.mode);
}


  
/* how long has this device been "on"? */
int Hardware::getTime() {
  return (this->timer == 0) ? 0 : (millis() - this->timer);
}

void Hardware::setValueByName(char *v) {
  int val = OFF;
  if(strcasecmp(v, "OFF") && strcasecmp(v, "CLOSED"))
    val = ON;
  this->setValue(val);
}

void Hardware::setValue(int val) {
  this->timer = (val != OFF) ? millis() : 0;
  this->val = val;
  // use a buffer?
  if(this->buffer) {
    uint8_t v = 1 << (this->address & 0xFF);
    if(val)
      (*(this->buffer)) |= v;
    else
      (*(this->buffer)) &= ~(v);
    val = *(this->buffer);
  }
  this->hw.setVal(this->address, val);
}

int Hardware::getValue() {
  int val = this->val;
  if(this->hw.getVal != NULL) {
    val = this->hw.getVal(this->address);
    if(this->convertFunc)
      val = this->convertFunc(val);
    this->val = val;
  }
  return this->val;
}
  



Param::Param(char *_name, int _address, int _default) : Object(TYPE_PARAM, _name)
{
  this->address = _address;
  //setValue(_default);
}
void Param::setValueByName(char *v) {
  int val = OFF;
  if(strcasecmp(v, "OFF") && strcasecmp(v, "CLOSED"))
    val = ON;
  this->setValue(val);
}
void Param::setValue(int val) {
  eepromWrite(this->address, (val >> 8));
  eepromWrite(this->address+1, (val & 0xFF));
}
int Param::getValue() {
  int val;
  val  = eepromRead(this->address) << 8;
  val |= eepromRead(this->address+1);
  return val;
}



/* parse commands from serial interface and try to interpret and execute them */
void serial() {
  byte i;
  char c = 0;
  char *v = NULL;
  Object *o = NULL;
  int len = strlen(serialbuffer);
  
  if(!serialAvailable()) {
    return;
  }

  /* read new data */
  while(serialAvailable()) {
    c = serialRead();
    serialbuffer[len++] = c;
    if(len >= (SBUF-1) || c == '?' || c == '!') {
      serialbuffer[len] == 0;
      break;
    }
  }
 

  /* Command complete? */
  if(c == '?' || c == '!') {
    /* extract value to set */
    if(c == '!') {
      for(i = 0; i < len-1; i++) {
        if(serialbuffer[i] == '=') {
          serialbuffer[i] = 0;
          v = &serialbuffer[i+1];
          break;
        }
      }
    }
    serialbuffer[len-1] = 0; // cut off name of object

    /* find object by name */
    for(o = ob[(byte)((serialbuffer[0] & OBHASHSIZE) ^ (serialbuffer[1] & OBHASHSIZE))]; o; o = o->getNext())
      if(!strcmp(o->getName(), serialbuffer))
        break;

    if(o && (c == '?' || (c == '!' && v))) {
      if(c == '?') {
        o->showValue();
      } else {
        c = 0;
        if(v[0] >= '0' && v[0] <= '9')
          o->setValue((int) (atoi(v)));
        else
          o->setValueByName((char *) v);
      }
    } else
      serialWrite("?");
  /* Command too long? */
  } else if(len >= (SBUF-1)) {   
    serialWrite("Too long!");
    goto end_serial;
  /* Command incomplete? */
  } else {
    return;
  }
end_serial:
  memset(serialbuffer, 0, SBUF);
}

