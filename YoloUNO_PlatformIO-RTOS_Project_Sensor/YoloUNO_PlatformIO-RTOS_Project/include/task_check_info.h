#ifndef __TASK_CHECK_INFO_H__
#define __TASK_CHECK_INFO_H__

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "global.h"

// Đọc cấu hình từ LittleFS vào struct (không dùng biến toàn cục)
bool loadNetConfig(NetConfig_t* outConfig);

void Delete_info_File();
void Save_info_File(String wifi_ssid, String wifi_pass, String token, String server, String port);
bool check_info_File(bool isLoopMode);

// Forward declare
void startAP();

#endif