#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include <ArduinoJson.h> 
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
// ─── Semaphore Độ Ẩm (Task 2 NeoPixel) ────────────────────────────────────────
extern SemaphoreHandle_t semHum0_30;
extern SemaphoreHandle_t semHum30_50;
extern SemaphoreHandle_t semHum50_80;
extern SemaphoreHandle_t semHum80_90;
extern SemaphoreHandle_t semHum90_100;


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

// LCD backlight control từ webserver (semaphore)
// Give = tắt backlight, Take = bật lại
extern SemaphoreHandle_t semLcdOff;


// ─── TinyML 3-Class Classification ──────────────────────────────────────────
enum MLClass { 
    BACKGROUND = 0, 
    FIRE = 1, 
    NUISANCE = 2 
};

// Metrics để lưu kết quả TinyML cho predict log
typedef struct {
    char time[9];
    float temp;
    float hum;
    uint8_t predicted_class;
    float confidence;
} LogEntry;
#define MAX_LOGS 20

// Chỉ lưu những gì Frontend cần
typedef struct {
    uint8_t predicted_class; 
    float confidence;        
} TinyMLMetrics;

extern SemaphoreHandle_t semTinyML;

void tinyml_lock();
void tinyml_unlock();

void initTinyMLSync();
void tinyml_update_all(uint8_t predicted_class, float confidence, float temp, float hum);

TinyMLMetrics tinyml_get_metrics();
int tinyml_get_logs_json(char* buffer, size_t maxSize);

#endif