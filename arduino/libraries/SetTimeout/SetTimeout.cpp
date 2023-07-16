#include "Arduino.h"
#include "SetTimeout.h"

#ifdef INC_FREERTOS_H
#include <freertos/FreeRTOS.h>
SemaphoreHandle_t xMutex = NULL;
inline void lock_init(){
	xMutex = xSemaphoreCreateMutex();
}
inline void lock_on(){
	const TickType_t xTicksToWait=1000UL;
	BaseType_t xStatus = xSemaphoreTake(xMutex, xTicksToWait);
}
inline void lock_off(){
	xSemaphoreGive(xMutex);	
}
#else   //mbedOS
#include <rtos.h>
static rtos::Mutex mutex;
inline void lock_init(){
}
inline void lock_on(){
  mutex.lock();
}
inline void lock_off(){
  mutex.unlock();
}
#endif //MBED_H

struct SetTimeoutTab{
  long timeout;
  uint8_t msg[8];
  int msglen;
  SetTimeoutCallback func;
  SetTimeoutTab(){
  	func=NULL;
    timeout=0;
    memset(msg,0,sizeof(msg));
    msglen=0;
  }
  SetTimeoutTab& operator =(SetTimeoutTab& t){
  	func=t.func;
    timeout=t.timeout;
    memcpy(msg,t.msg,sizeof(msg));
    msglen=t.msglen;
  }
};

SetTimeoutClass::SetTimeoutClass(void){
  tbl=new SetTimeoutTab[20];
  lock_init();
}
long SetTimeoutClass::set(SetTimeoutCallback f,int ms){
  return set(NULL,-1,(SetTimeoutCallbackPN)f,ms);
}
long SetTimeoutClass::set(char *s,SetTimeoutCallbackP f,int ms){
  return set((uint8_t *)s,0,(SetTimeoutCallbackPN)f,ms);
}
long SetTimeoutClass::set(uint8_t *s,int l,SetTimeoutCallbackPN f,int ms){
  lock_on();
  long now=micros();
  long tout=now+(((long)ms)<<10);
  int n=0;
  for(;;n++){
    if(tbl[n].func==NULL) break;
    long diff=tout-tbl[n].timeout;
    if(diff<0){
      SetTimeoutTab tt1=tbl[n],tt2=tbl[n+1];
      for(int i=n+1;;i++){
        tt2=tbl[i];
        tbl[i]=tt1;
      	if(tbl[i].func==NULL) break;
        tt1=tt2;
      }
      break;
    }
  }
  tbl[n].func=(SetTimeoutCallback)f;
  tbl[n].timeout=tout;
  tbl[n].msglen=l;
  if(s!=NULL){
  	if(l==0) strcpy((char *)tbl[n].msg,(char *)s);
  	else memcpy(tbl[n].msg,s,l);
  }
  lock_off();
  return tout;
}
int SetTimeoutClass::clear(long d){
  lock_on();
  int n=this->lookup(d);
  if(n>=0){
    for(int i=n;;i++){
      tbl[i]=tbl[i+1];
      if(tbl[i].func==NULL) break;
    }
  }
  lock_off();
  return n;
}
int SetTimeoutClass::lookup(long d){
  SetTimeoutTab *et=tbl;
  for(int n=0;;n++,et++){
    if(et->func==NULL) return -n-1;
    else if(et->timeout==d) return n;
  }
}
SetTimeoutCallback SetTimeoutClass::spinOnce(void){
  lock_on();
  SetTimeoutTab et=tbl[0];
  long now=micros();
  long diff=et.timeout-now;
  if(diff<0){
    for(int i=0;;i++){
      tbl[i]=tbl[i+1];
      if(tbl[i].func==NULL) break;
    }
  }
  else et.func=NULL;
  lock_off();
  if(et.func!=NULL){
    if(et.msglen>0) (*(SetTimeoutCallbackPN)et.func)(et.msg,et.msglen);
    else if(et.msglen==0) (*(SetTimeoutCallbackP)et.func)((char *)et.msg);
    else (*(SetTimeoutCallback)et.func)();
  }
  return et.func;
}

SetTimeoutClass setTimeout;
