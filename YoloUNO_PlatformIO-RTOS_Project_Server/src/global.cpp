#include "global.h"

// ─── Queue (size=1, overwrite mode dùng xQueueOverwrite) ─────────────────────
// Luôn giữ giá trị sensor MỚI NHẤT, không block producer
QueueHandle_t xQueueSensorData   = xQueueCreate(1, sizeof(SensorData));

// ─── Semaphore Nhiệt Độ ───────────────────────────────────────────────────────
// Tại một thời điểm chỉ có 1 trong 3 semaphore được Give (Active)
SemaphoreHandle_t semTempNormal   = xSemaphoreCreateBinary();
SemaphoreHandle_t semTempWarning  = xSemaphoreCreateBinary();
SemaphoreHandle_t semTempCritical = xSemaphoreCreateBinary();

// ─── Semaphore Độ Ẩm ─────────────────────────────────────────────────────────
SemaphoreHandle_t semHumNormal    = xSemaphoreCreateBinary();
SemaphoreHandle_t semHumWarning   = xSemaphoreCreateBinary();
SemaphoreHandle_t semHumCritical  = xSemaphoreCreateBinary();

// ─── WiFi / CoreIOT Config ─────────────────────────────────────────────────────
EventGroupHandle_t egWifiStatus = NULL;

// ─── LCD Backlight Control ─────────────────────────────────────────────────────
// Binary semaphore: Give = yêu cầu tắt, không có = bật
SemaphoreHandle_t semLcdOff = xSemaphoreCreateBinary();