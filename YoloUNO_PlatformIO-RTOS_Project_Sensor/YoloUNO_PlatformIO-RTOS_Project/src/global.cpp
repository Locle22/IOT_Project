#include "global.h"

// ─── Queue (size=1, overwrite mode) ──────────────────────────────────────────
QueueHandle_t xQueueSensorData = xQueueCreate(1, sizeof(SensorData));

// ─── WiFi EventGroup ─────────────────────────────────────────────────────────
EventGroupHandle_t egWifiStatus = NULL;