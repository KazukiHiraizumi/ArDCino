#ifndef _Algor_h
#define _Algor_h

#include  "Arduino.h"

extern void algor_prepare();
extern uint16_t algor_update(int32_t time,int32_t duty);
extern uint8_t algor_param[8*7];

#endif
