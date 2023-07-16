#ifndef _Ble_h
#define _Ble_h

#include  "Arduino.h"

namespace ble{
  void run(char *deviceName,char *serviceUuid,char *requestCharUuid,char *notifyCharUuid);
  void logdump();
  extern uint8_t led_pin;
  extern bool led_invert;
  extern bool enb_connect;
  extern bool flag_connect;
}

#endif
