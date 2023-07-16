#ifndef _Dcore_h
#define _Dcore_h

#include  "Arduino.h"

namespace dcore{
  typedef void (*StartCB_t)();
  typedef uint16_t (*ContCB_t)(int32_t interval_usec,int32_t turnon_usec);
  typedef void (*EndCB_t)();
  extern uint8_t RunLevel,tpwm;
  void run(int sens,int gate,StartCB_t,ContCB_t,EndCB_t);
  void shift();  //force mode shift approprite for current mode
  void sleep(uint16_t ms);
}

#endif
