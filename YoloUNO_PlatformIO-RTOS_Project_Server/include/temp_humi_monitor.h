#ifndef __TEMP_HUMI_MONITOR__
#define __TEMP_HUMI_MONITOR__
#include <Arduino.h>
#include <Wire.h>
#include "LiquidCrystal_I2C.h"
#include "global.h"

// Task 3: Hiển thị dữ liệu nhận từ xQueueSensorData lên LCD1602
// Dữ liệu đến từ ESP32-A qua CoreIOT Shared Attributes
void temp_humi_monitor(void *pvParameters);

#endif