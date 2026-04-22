#ifndef __TASK_WIFI_H__
#define __TASK_WIFI_H__

#include <WiFi.h>
#include "global.h"

extern bool Wifi_reconnect();
extern void startAP();
extern void startSTA();

#endif