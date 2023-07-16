#include "Arduino.h"
#include <mbed.h>
#include <rtos.h>
#include <SetTimeout.h>
#include "Param.h"
#define NANO33BLE_FS_SIZE_KB        64
#define FORCE_REFORMAT              false
#include <FS_Nano33BLE.h>

namespace param{
  FileSystem_MBED *myFS=NULL;
  static char *path=MBED_FS_FILE_PREFIX "/param.bin";
  uint8_t *data;
  static uint16_t data_len;
  int length(){ return data_len;}
  void task_load(){
    FILE *file = fopen(path, "r");
    if(file){
      int num = fread(data, 1, data_len, file);
      fclose(file);
      Serial.println("Param load done");
    }
    else{
      Serial.println("Param load failed");
    }
  }
  void task_dump(){
    FILE *file = fopen(path, "w");
    if(file){
      int num = fwrite(data, 1, data_len, file);
      fclose(file);
      Serial.println("Param dump done");
    }
    else{
      Serial.println("Param dump failed");
    }
  }
  void load(){
    setTimeout.set(task_load,0);
  }
  void dump(){
    setTimeout.set(task_dump,0);
  }
  void run(uint8_t *param,int len){
    data=param;
    data_len=len;
    uint16_t start=millis();
    if(myFS==NULL){
      myFS = new FileSystem_MBED();
      if (!myFS->init()){
        Serial.println("FS Mount Failed");
        return;
      }
    }
    task_load();
    Serial.print("param init done ");
    Serial.println(millis()-start);
  }
}
