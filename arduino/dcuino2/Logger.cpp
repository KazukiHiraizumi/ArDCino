#include "Arduino.h"
#include "Logger.h"
#include <SetTimeout.h>

namespace logger{
  static struct ALOG buf[1000];
  struct ALOG *data=buf;
  struct ALOG stage;
  static int dmpidx;
  static void (*dmpf)();
  static int32_t tzero;
  int size=0;
  void clear(){
    memset(&stage,0,sizeof(stage));
  }
  int limit(){
    return sizeof(buf)/sizeof(ALOG);
  }
  int length(){
    return size;
  }
  void start(){
	  clear();
	  size=0;
    tzero=micros();
  }
  void latch(){
	  if(stage.stamp){
	    buf[size++]=stage;
  	  clear();
	  }
    stage.stamp=micros()-tzero;
  }
  void dump(){
    if(!Serial) return;
    dmpidx=0;
    setTimeout.set(dmpf=[](){
      auto t0=millis();
      for(;millis()-t0<50;){
        ALOG *logs=data+dmpidx;
        Serial.print(dmpidx);
        Serial.print("/");
        Serial.print(size);
        Serial.print(")");
        Serial.print(logs->stamp);
        Serial.print(" ");
        Serial.print(logs->interval);
        Serial.print(" ");
        Serial.print(logs->mode);
        Serial.print(" ");
        Serial.print(logs->latency);
        Serial.print(" ");
        Serial.print(logs->omega);
        Serial.print(" ");
        Serial.print(logs->beta);
        Serial.print(" ");
        Serial.print(logs->sigma);
        Serial.print(" ");
        Serial.print(logs->eval);
        Serial.print(" ");
        Serial.print(logs->duty);
        Serial.print(" ");
        Serial.println(logs->cmd);
        dmpidx++;
        if(dmpidx>=size) return;
      }
      setTimeout.set(dmpf,10);
    },10);
  }
}
