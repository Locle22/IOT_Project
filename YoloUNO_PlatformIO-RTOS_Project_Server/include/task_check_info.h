#ifndef __TASK_CHECK_INFO_H__
#define __TASK_CHECK_INFO_H__

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "task_wifi.h"
#include "global.h"

// Xử lý nạp cấu hình từ LittleFS ra tham số struct (không dùng biến toàn cục)
bool loadNetConfig(NetConfig_t* outConfig);

void Delete_info_File();
void Save_info_File(String wifi_ssid, String wifi_pass, String CORE_IOT_TOKEN, String CORE_IOT_SERVER, String CORE_IOT_PORT);
bool check_info_File(bool isLoopMode);

#endif