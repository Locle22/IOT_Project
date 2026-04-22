#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

// ─── Sensor Data ───────────────────────────────────────────────────────────────
// Struct truyền dữ liệu cảm biến giữa các task (thay thế biến toàn cục)
typedef struct {
    float temp;  // °C
    float hum;   // %
} SensorData;

// Queue kích thước 1 — luôn giữ giá trị MỚI NHẤT
extern QueueHandle_t xQueueSensorData;

// ─── Semaphore Nhiệt Độ (Task 1 LED + Task 3 LCD) ─────────────────────────────
// Ngưỡng: Normal < 30°C | Warning 30-38°C | Critical >= 38°C
extern SemaphoreHandle_t semTempNormal;
extern SemaphoreHandle_t semTempWarning;
extern SemaphoreHandle_t semTempCritical;

// ─── Semaphore Độ Ẩm (Task 2 NeoPixel) ────────────────────────────────────────
// Ngưỡng: Normal < 60% | Warning 60-80% | Critical >= 80%
extern SemaphoreHandle_t semHumNormal;
extern SemaphoreHandle_t semHumWarning;
extern SemaphoreHandle_t semHumCritical;

// ─── WiFi / CoreIOT Config ─────────────────────────────────────────────────────
// Cấu trúc chứa thông tin đăng nhập và máy chủ CoreIOT
typedef struct {
    char ssid[32];
    char pass[64];
    char coreToken[64];
    char coreServer[64];
    int corePort;
} NetConfig_t;

// Dùng EventGroup thay thế cho cờ toàn cục và BinarySemaphore (chuẩn FreeRTOS)
#include "freertos/event_groups.h"
extern EventGroupHandle_t egWifiStatus;
#define WIFI_CONNECTED_BIT BIT0

#endif