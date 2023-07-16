#include <ArduinoBLE.h>
#include <mbed.h>
#include <rtos.h>
#include <SetTimeout.h>
#include "Dcore.h"
#include "Logger.h"
#include "Param.h"
#include "Ble.h"

#define PACKET_LEN 10 //charasteristic packet length
#define notifyCharacteristic (*notifyCharacteristicPtr)
static uint16_t chendian(uint16_t b){
  uint16_t a;
  uint8_t *p=(byte *)&a;
  p[0]=b>>8;
  p[1]=b&0xff;
  return a;
}

namespace ble{
  static rtos::Thread thread_alive;
  static const char* deviceName;
  static const char* serviceUuid;
  static const char* requestCharUuid;
  static const char* notifyCharUuid;
  static BLECharacteristic* notifyCharacteristicPtr;
  static int16_t sweep_param=-1;
  static int16_t sweep_logger=-1;
  static long sweep_queue=0;
  static inline bool sweep_busy(){ return (int16_t)(sweep_logger&sweep_param)>=0;}
//public:
  bool enb_connect=true;
  bool flag_connect=false;
  uint8_t led_pin=LED_PWR;
  bool led_invert=false;
  void sweep_callback() {
    uint8_t buf[10];
    if(sweep_param>=0){
      Serial.print("Sweep param ");
      Serial.println(sweep_param);
      buf[0]=0xB0;
      buf[1]=sweep_param;
      int i=2;
      for(;i<sizeof(buf);i++,sweep_param++){
        if(sweep_param>=param::length()) break;
        buf[i]=param::data[sweep_param];
      }
      if(i>2){
        notifyCharacteristic.writeValue(buf,i);
      }
      if(sweep_param<param::length()){
        sweep_queue=setTimeout.set(sweep_callback,20);
      }
      else sweep_param=-1;
    }
    else if(sweep_logger>=0){
      Serial.print("Sweep logger ");
      long i=0,srev=0,stm=0,sdt=0,spwm=0,sval=0,stens=0;
      for(;i<3;i++,sweep_logger++){
        if(sweep_logger>=logger::length()) break;
        logger::ALOG alog=logger::data[sweep_logger];
        srev+=sweep_logger;
        stm+=alog.stamp/1000;
        sdt+=alog.interval;
        spwm+=alog.duty;
        sval+=alog.eval;
        stens+=alog.beta;
      }
      if(i>0){
        uint16_t *p=(uint16_t *)buf;
        p[0]=chendian(srev/i);
        p[1]=chendian(stm/i);
        p[2]=chendian(sdt/i);
        ((uint8_t *)(p+3))[0]=spwm/i;
        ((uint8_t *)(p+3))[1]=sval/i;
        p[4]=chendian(stens/i);
        Serial.print(srev/i);Serial.print(",");
        Serial.println(stens/i);
        notifyCharacteristic.writeValue(buf,10);
      }
      if(sweep_logger<logger::length()){
        sweep_queue=setTimeout.set(sweep_callback,20);
      }
      else sweep_logger=-1;
    }
  }
  void request_callback(BLEDevice central, BLECharacteristic characteristic) {
    uint8_t buf[10];
    int len=characteristic.readValue(buf,sizeof(buf));
    if(len==3 && buf[0]==0xA0){//Request Rom Write
      Serial.println("Req param write");
      if(buf[1]<param::length()){
        param::data[buf[1]]=buf[2];
        buf[0]=0xA0;
      }
      else{
        buf[0]=0xAF;
        buf[1]=0xFF;      
      }
      setTimeout.set(buf,2,[](uint8_t *s,int n){
        notifyCharacteristic.writeValue(s,n);
      },200);
    }
    else if(len==2 && buf[0]==0xFA){//Request Rom Dump
      Serial.println("Req param dump");
      if(buf[1]&4) param::dump();
      else if(buf[1]&2){
        param::load();
        delay(10);//rtos::ThisThread::sleep_for(10);
      }
      if(buf[1]&1){
        sweep_param=0;
        sweep_queue=setTimeout.set(sweep_callback,10);
      }
    }
    else if(len==2 && buf[0]==0xFC && buf[1]==0x01){//Request Wave Dump
      Serial.println("Req wav dump");
      logdump();
    }
  }
  void logdump(){
    if(flag_connect){
      sweep_logger=0;
      sweep_queue=setTimeout.set(sweep_callback,10);
    }
  }
  void led_run(){
    digitalWrite(led_pin,led_invert? LOW:HIGH);//digitalWrite(LEDR,LOW);
    setTimeout.set([](){  //to turn off
      digitalWrite(led_pin,led_invert? HIGH:LOW);//digitalWrite(LEDR,HIGH);
    },10);
    setTimeout.set(led_run,flag_connect? 3000:1000);  //to turn on next
  }
  void chk_connect(){
    BLEDevice central = BLE.central();
    if(central) {
      Serial.print("Connected! MAC address: ");
      Serial.println(central.address());
      flag_connect=true;
      while(1){
        if(sweep_busy()) delay(100);//rtos::ThisThread::sleep_for(100);
        else dcore::sleep(100);
        if(!central.connected()) break;
      }
      flag_connect=false;
    }
  }
  void task_alive(){
    delay(3000);//rtos::ThisThread::sleep_for(3000);
    if (!BLE.begin()) {
      Serial.println("Starting Bluetooth® Low Energy module failed!");
      return;
    }
    BLE.setLocalName(deviceName);
    BLEService Service(serviceUuid);
    BLEStringCharacteristic requestChara(requestCharUuid, BLEWrite, PACKET_LEN);
    BLEStringCharacteristic notifyChara(notifyCharUuid, BLERead|BLENotify, PACKET_LEN);
    notifyCharacteristicPtr=&notifyChara;

    BLE.setAdvertisedService(Service);
    Service.addCharacteristic(requestChara);
    Service.addCharacteristic(notifyChara);
    BLE.addService(Service);
    requestChara.setEventHandler(BLEWritten, request_callback);

    BLE.advertise();
    Serial.println("BLE start advertising");
    while(1){
      if(enb_connect) chk_connect();
      dcore::sleep(1000);//delay(1000);//rtos::ThisThread::sleep_for(1000);
    }
  }
  void run(char *name,char *svc,char *chr_req,char *chr_not){
    deviceName=name;
    serviceUuid=svc;
    requestCharUuid=chr_req;
    notifyCharUuid=chr_not;
    flag_connect=false;
    led_run();
    thread_alive.start(mbed::callback(task_alive));
  }
}
