#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

// ─── Sensor Data ───────────────────────────────────────────────────────────────
typedef struct {
    float temp;  // °C
    float hum;   // %
} SensorData;

// Queue kích thước 1 — luôn giữ giá trị MỚI NHẤT
extern QueueHandle_t xQueueSensorData;

// ─── WiFi / CoreIOT Config ─────────────────────────────────────────────────────
typedef struct {
    char ssid[32];
    char pass[64];
    char coreToken[64];
    char coreServer[64];
    int corePort;
} NetConfig_t;

// EventGroup quản lý trạng thái WiFi (chuẩn FreeRTOS)
extern EventGroupHandle_t egWifiStatus;
#define WIFI_CONNECTED_BIT BIT0

#endif