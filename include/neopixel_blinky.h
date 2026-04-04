#ifndef NEOPIXEL_BLINKY_H
#define NEOPIXEL_BLINKY_H

#include <Arduino.h>
#include "DHT20.h"

// Đưa struct ra ngoài để hàm main có thể khởi tạo
struct SensorData {
    DHT20* dht20;
    uint8_t humiRange;
    SemaphoreHandle_t mutexData;
    SemaphoreHandle_t semaNeoPixelUpdate;
};

// Khai báo prototype của 2 task
void sensorRead(void *pvParameters);
void neo_blinky(void *pvParameters);

#endif