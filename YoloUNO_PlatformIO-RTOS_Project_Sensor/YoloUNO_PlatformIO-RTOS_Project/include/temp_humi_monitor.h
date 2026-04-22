#ifndef __TEMP_HUMI_MONITOR__
#define __TEMP_HUMI_MONITOR__

#include <Arduino.h>
#include <Wire.h>
#include <DHT20.h>
#include "global.h"

// Task: đọc DHT20 → ghi vào xQueueSensorData
void temp_humi_monitor(void *pvParameters);

#endif