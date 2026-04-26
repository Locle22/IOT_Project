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

// ─── TinyML Mutex và Queue ─────────────────────────────────────────────────
// Mutex bảo vệ truy cập metrics và log buffer
SemaphoreHandle_t semTinyML = NULL;

// Queue trả kết quả inference cho các task khác
QueueHandle_t xQueueTinyMLResult = NULL;

// TinyML Metrics (protected by semTinyML)
static TinyMLMetrics s_tinyml_metrics = {0, 0, 0, 0};

// ─── TinyML Thread-Safe Functions ──────────────────────────────────────────
void tinyml_lock() {
    if (semTinyML != NULL) {
        xSemaphoreTake(semTinyML, portMAX_DELAY);
    }
}

void tinyml_unlock() {
    if (semTinyML != NULL) {
        xSemaphoreGive(semTinyML);
    }
}

TinyMLMetrics tinyml_get_metrics() {
    TinyMLMetrics result;
    tinyml_lock();
    result = s_tinyml_metrics;
    tinyml_unlock();
    return result;
}

void tinyml_update_metrics(uint8_t predicted_class, uint32_t duration_us, uint32_t arena_used_bytes) {
    tinyml_lock();
    s_tinyml_metrics.predicted_class = predicted_class;
    s_tinyml_metrics.last_inference_time_us = duration_us;
    s_tinyml_metrics.arena_used_bytes = arena_used_bytes;
    if (predicted_class == FIRE) {
        s_tinyml_metrics.last_fire_time = millis();
    } else if (predicted_class == NUISANCE) {
        s_tinyml_metrics.last_nuisance_time = millis();
    }
    tinyml_unlock();
}

// ─── Khởi tạo TinyML Mutex và Queue ─────────────────────────────────────────
void initTinyMLSync() {
    semTinyML = xSemaphoreCreateMutex();
    xQueueTinyMLResult = xQueueCreate(TINYML_QUEUE_SIZE, sizeof(TinyMLResult));
}