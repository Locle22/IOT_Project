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

// ─── TinyML 3-Class Classification ──────────────────────────────────────────
// Enum for TinyML output classes
enum MLClass { 
    BACKGROUND = 0, 
    FIRE = 1, 
    NUISANCE = 2 
};

// TinyML Metrics (protected by semTinyML)
typedef struct {
    uint8_t predicted_class;            // 0=Background, 1=Fire, 2=Nuisance
    uint32_t last_inference_time_us;    // Inference time in microseconds
    uint32_t arena_used_bytes;          // Tensor arena memory usage
    uint32_t last_fire_time;            // Timestamp when FIRE was detected
    uint32_t last_nuisance_time;        // Timestamp when NUISANCE was detected
} TinyMLMetrics;

// TinyML Mutex - bảo vệ truy cập metrics
extern SemaphoreHandle_t semTinyML;

// TinyML Queue - trả kết quả inference cho các task khác
// Kích thước 5, overwrite mode để luôn giữ kết quả mới nhất
#define TINYML_QUEUE_SIZE 5
typedef struct {
    uint8_t predicted_class;
    uint32_t timestamp;
    uint32_t duration_us;
    uint32_t arena_used_bytes;
    float temp;
    float hum;
} TinyMLResult;
extern QueueHandle_t xQueueTinyMLResult;

// Functions để truy cập TinyML metrics một cách thread-safe
void tinyml_lock();
void tinyml_unlock();
TinyMLMetrics tinyml_get_metrics();
void tinyml_update_metrics(uint8_t predicted_class, uint32_t duration_us, uint32_t arena_used_bytes);
void initTinyMLSync();

#endif