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
SemaphoreHandle_t semHum90_100   = xSemaphoreCreateBinary();
SemaphoreHandle_t semHum80_90    = xSemaphoreCreateBinary();
SemaphoreHandle_t semHum50_80    = xSemaphoreCreateBinary();
SemaphoreHandle_t semHum30_50    = xSemaphoreCreateBinary();
SemaphoreHandle_t semHum0_30     = xSemaphoreCreateBinary();

// ─── WiFi / CoreIOT Config ─────────────────────────────────────────────────────
EventGroupHandle_t egWifiStatus = NULL;

// ─── LCD Backlight Control ─────────────────────────────────────────────────────
// Binary semaphore: Give = yêu cầu tắt, không có = bật
SemaphoreHandle_t semLcdOff = xSemaphoreCreateBinary();

// ─── TinyML State & Circular Buffer ─────────────────────────────────────────
SemaphoreHandle_t semTinyML = NULL;

static TinyMLMetrics s_tinyml_metrics = {0, 0.0f};
static LogEntry s_log_buffer[MAX_LOGS];
static int s_log_head = 0;  // next write position
static int s_log_count = 0;

void initTinyMLSync() {
    semTinyML = xSemaphoreCreateMutex();
    memset(s_log_buffer, 0, sizeof(s_log_buffer));
}

void tinyml_lock() {
    if (semTinyML != NULL) xSemaphoreTake(semTinyML, portMAX_DELAY);
}

void tinyml_unlock() {
    if (semTinyML != NULL) xSemaphoreGive(semTinyML);
}

// funct update metrics and log entry
void tinyml_update_all(uint8_t predicted_class, float confidence, float temp, float hum) {
    tinyml_lock();
    
    s_tinyml_metrics.predicted_class = predicted_class;
    s_tinyml_metrics.confidence = confidence;

    LogEntry* entry = &s_log_buffer[s_log_head];
    
    unsigned long s = millis() / 1000;
    snprintf(entry->time, sizeof(entry->time), "%02lu:%02lu:%02lu", (s/3600)%24, (s/60)%60, s%60);
    
    entry->temp = temp;
    entry->hum = hum;
    entry->predicted_class = predicted_class;
    entry->confidence = confidence;

    s_log_head = (s_log_head + 1) % MAX_LOGS;
    if (s_log_count < MAX_LOGS) s_log_count++;

    tinyml_unlock();
}

TinyMLMetrics tinyml_get_metrics() {
    TinyMLMetrics result;
    tinyml_lock();
    result = s_tinyml_metrics;
    tinyml_unlock();
    return result;
}

// Convert array of LogEntry to JSON array string for Webserver
int tinyml_get_logs_json(char* buffer, size_t maxSize) {
    tinyml_lock();
    
    StaticJsonDocument<2048> doc;
    JsonArray array = doc.to<JsonArray>();

    for (int i = 0; i < s_log_count; i++) {
        int idx = (s_log_head - 1 - i + MAX_LOGS) % MAX_LOGS;
        JsonObject obj = array.createNestedObject();
        obj["time"] = s_log_buffer[idx].time;
        obj["temp"] = s_log_buffer[idx].temp;
        obj["hum"]  = s_log_buffer[idx].hum;
        obj["class"] = s_log_buffer[idx].predicted_class;
        obj["conf"] = s_log_buffer[idx].confidence;
    }

    int bytes = serializeJson(doc, buffer, maxSize);
    tinyml_unlock();
    return bytes;
}