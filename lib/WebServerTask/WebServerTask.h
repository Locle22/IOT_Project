#ifndef WEBSERVER_TASK_H
#define WEBSERVER_TASK_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "SensorTask.h"
#include "LCDTask.h"
#include "FanMonitor.h"

// WiFi credentials
extern const char* ssid;
extern const char* password;

// Hàm điều khiển LCD backlight
void LCDBacklightON();
void LCDBacklightOFF();

// Khởi tạo và task
void initWebServer(const char* wifiSSID, const char* wifiPassword);
void TaskWebServer(void *pvParameters);

#endif
