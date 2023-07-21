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
#define SEEEDXIAO

#ifdef ARDUINO_ARDUINO_NANO33BLE
#define DI_SENS D10
#define DO_FET D2
#endif
#if defined(ARDUINO_SEEED_XIAO_NRF52840) || defined(ARDUINO_SEEED_XIAO_NRF52840_SENSE)
//#define DI_SENS D1
//#define DO_FET D0
#define DI_SENS D8
#define DO_FET D5
#endif

void setup() {
//  pinMode(LEDG,OUTPUT);
//  digitalWrite(LEDG,LOW);
  Serial.begin(115200);
  digitalWrite(LED_PWR,LOW);//Power LED Turn off

  pinMode(DI_SENS,INPUT);  //rotation sensor
  pinMode(DO_FET,OUTPUT);  //FET
  dcore::run(DI_SENS,DO_FET,
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

  param::run(algor_param,sizeof(algor_param));

  ble::run(
    "OpenDCb",  //device name 
    "10014246-f5b0-e881-09ab-42000ba24f83",  //service uuid
    "20024246-f5b0-e881-09ab-42000ba24f83",  //request uuid
    "20054246-f5b0-e881-09ab-42000ba24f83"   //notification uuid
  );
  ble::led_pin=LED_PWR;
#if defined(ARDUINO_SEEED_XIAO_NRF52840) || defined(ARDUINO_SEEED_XIAO_NRF52840_SENSE)
  ble::led_invert=true;
#endif
  pinMode(LEDB,INPUT);
}

void loop() {
  if(setTimeout.spinOnce()==NULL){
    dcore::sleep(10);
    NRF_WDT->RR[0]=WDT_RR_RR_Reload;
  }
}
