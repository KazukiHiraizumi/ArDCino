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

#define PRM_ReadData(n) ((uint32_t)param::data[n])
#define PRM_ReadData10x(n) ((uint32_t)param::data[n]*10)
#define PRM_ReadData100x(n) ((uint32_t)param::data[n]*100)
#define PRM_ReadData1000x(n) ((uint32_t)param::data[n]*1000)

#endif
