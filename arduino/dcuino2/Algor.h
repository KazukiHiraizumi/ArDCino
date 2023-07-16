#ifndef _Algor_h
#define _Algor_h

#include  "Arduino.h"

extern void algor_prepare();
extern uint16_t algor_update(int32_t time,int32_t duty);
extern uint8_t algor_param[8*7];

template<typename T> inline long accumulate(T *arr1,T *arr2){
  long sum=0;
  for(;arr1<arr2;arr1++) sum+=*arr1;
  return sum;
}

#endif
