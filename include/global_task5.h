#ifndef GLOBAL_TASK5_H
#define GLOBAL_TASK5_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Define a structure to hold sensor data
struct SensorData {
    float temperature;
    float humidity;
};

extern QueueHandle_t sensorQueue;

#endif