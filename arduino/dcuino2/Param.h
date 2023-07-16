#ifndef _Param_h
#define _Para,_h
#include  "Arduino.h"

namespace param{
  extern uint8_t *data;
  int length();
  void dump();
  void load();
  void run(uint8_t *param_table,int table_length);
}

#endif
