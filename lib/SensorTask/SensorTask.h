#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include <Arduino.h>
#include <Wire.h>
#include "DHT20.h"

// Semaphores cho nhiệt độ
extern SemaphoreHandle_t semTempCold;      // < 18°C
extern SemaphoreHandle_t semTempNormal;    // 18-28°C
extern SemaphoreHandle_t semTempElevated;  // 28-35°C
extern SemaphoreHandle_t semTempCritical;  // >= 35°C

// Semaphores cho độ ẩm
extern SemaphoreHandle_t semHumDry;        // < 30%
extern SemaphoreHandle_t semHumOptimal;    // 30-60%
extern SemaphoreHandle_t semHumHumid;      // 60-80%
extern SemaphoreHandle_t semHumExtreme;    // >= 80%

extern SemaphoreHandle_t i2cMutex;
extern DHT20 dht20;

void initSensorSemaphores();
void TaskSensorRead(void *pvParameters);

#endif
