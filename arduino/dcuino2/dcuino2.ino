//
//PIN Assign:https://github.com/arduino/ArduinoCore-nRF528x-mbedos/blob/master/variants/ARDUINO_NANO33BLE/pins_arduino.h
//
#include <Arduino.h>
#include <mbed.h>
#include <mbed_boot.h>
#include <rtos.h>
#include <SetTimeout.h>
#include "Dcore.h"
#include "Logger.h"
#include "Param.h"
#include "Algor.h"
#include "Ble.h"

//#define NANO33
//#define SEEEDXIAO

//#ifdef ARDUINO_ARDUINO_NANO33BLE
//#define DI_SENS D10
//#define DO_FET D2
//#endif
//#if defined(ARDUINO_SEEED_XIAO_NRF52840) || defined(ARDUINO_SEEED_XIAO_NRF52840_SENSE)
//#define DI_SENS D8
//#define DO_FET D5
//#endif

int getPin(int n){
  switch(n){
    case 0: return D0;
    case 1: return D1;
    case 2: return D2;
    case 3: return D3;
    case 4: return D4;
    case 5: return D5;
    case 6: return D6;
    case 7: return D7;
    case 8: return D8;
    case 9: return D9;
    case 10: return D10;
  }
}

void setup() {
  Serial.begin(115200);
  param::run(algor_param,sizeof(algor_param));

  int di_sen=getPin(PRM_ReadData(0));
  int do_fet=getPin(PRM_ReadData(1));
  pinMode(di_sen,INPUT);  //rotation sensor
  pinMode(do_fet,OUTPUT);  //FET
  dcore::run(di_sen,do_fet,
    [](){//start callback
      algor_prepare();
      NRF_WDT->CONFIG=0x01;     // Configure WDT to run when CPU is asleep
      NRF_WDT->CRV=3276L*3;      // Timeout[s] = (CRV-1)/32768
      NRF_WDT->RREN=0x01;       // Enable the RR[0] reload register
      NRF_WDT->TASKS_START=1;   // Start WDT
    },
    [](int32_t dt,int32_t on_dt){//cyclic callback
      uint16_t d=algor_update(dt,on_dt);
      return d;
    },
    [](){//end callback 
      ble::logdump();
      logger::dump();
    }
  );

//  while (!Serial);

  ble::run(
    "arDCino",  //device name 
    "10014246-f5b0-e881-09ab-42000ba24f83",  //service uuid
    "20024246-f5b0-e881-09ab-42000ba24f83",  //request uuid
    "20054246-f5b0-e881-09ab-42000ba24f83"   //notification uuid
  );
  ble::led_pin=LED_PWR;
#if defined(ARDUINO_SEEED_XIAO_NRF52840) || defined(ARDUINO_SEEED_XIAO_NRF52840_SENSE)
  ble::led_invert=true;
#endif
  pinMode(LEDB,INPUT);
  pinMode(LED_PWR, OUTPUT);
  digitalWrite(LED_PWR,HIGH);//Power LED Turn off
}

void loop() {
  if(setTimeout.spinOnce()==NULL){
    if(millis()>500) dcore::sleep(10);
    NRF_WDT->RR[0]=WDT_RR_RR_Reload;
  }
}
