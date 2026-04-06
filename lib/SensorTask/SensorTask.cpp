#include "SensorTask.h"

// Semaphores cho nhiệt độ
SemaphoreHandle_t semTempCold = NULL;
SemaphoreHandle_t semTempNormal = NULL;
SemaphoreHandle_t semTempElevated = NULL;
SemaphoreHandle_t semTempCritical = NULL;

// Semaphores cho độ ẩm
SemaphoreHandle_t semHumDry = NULL;
SemaphoreHandle_t semHumOptimal = NULL;
SemaphoreHandle_t semHumHumid = NULL;
SemaphoreHandle_t semHumExtreme = NULL;

SemaphoreHandle_t i2cMutex = NULL;
DHT20 dht20;

void initSensorSemaphores() {
    // Nhiệt độ
    semTempCold = xSemaphoreCreateBinary();
    semTempNormal = xSemaphoreCreateBinary();
    semTempElevated = xSemaphoreCreateBinary();
    semTempCritical = xSemaphoreCreateBinary();
    
    // Độ ẩm
    semHumDry = xSemaphoreCreateBinary();
    semHumOptimal = xSemaphoreCreateBinary();
    semHumHumid = xSemaphoreCreateBinary();
    semHumExtreme = xSemaphoreCreateBinary();
    
    // I2C Mutex
    i2cMutex = xSemaphoreCreateMutex();
}

void TaskSensorRead(void *pvParameters) {
    double temp, hum;
    
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
        dht20.begin();
        xSemaphoreGive(i2cMutex);
    }
    
    while(1) {
        if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
            dht20.read();
            temp = dht20.getTemperature(); 
            hum = dht20.getHumidity();
            xSemaphoreGive(i2cMutex);
        }

        // Xử lý nhiệt độ
        if (temp < 18.0) {
            xSemaphoreGive(semTempCold);
            Serial.print("Temp: "); Serial.print(temp); Serial.println("C -> COLD");
        } 
        else if (temp < 28.0) {
            xSemaphoreGive(semTempNormal);
            Serial.print("Temp: "); Serial.print(temp); Serial.println("C -> NORMAL");
        } 
        else if (temp < 35.0) {
            xSemaphoreGive(semTempElevated);
            Serial.print("Temp: "); Serial.print(temp); Serial.println("C -> ELEVATED");
        } 
        else {
            xSemaphoreGive(semTempCritical);
            Serial.print("Temp: "); Serial.print(temp); Serial.println("C -> CRITICAL");
        }

        // Xử lý độ ẩm
        if (hum < 30.0) {
            xSemaphoreGive(semHumDry);
            Serial.print("Hum: "); Serial.print(hum); Serial.println("% -> DRY");
        } 
        else if (hum < 60.0) {
            xSemaphoreGive(semHumOptimal);
            Serial.print("Hum: "); Serial.print(hum); Serial.println("% -> OPTIMAL");
        } 
        else if (hum < 80.0) {
            xSemaphoreGive(semHumHumid);
            Serial.print("Hum: "); Serial.print(hum); Serial.println("% -> HUMID");
        } 
        else {
            xSemaphoreGive(semHumExtreme);
            Serial.print("Hum: "); Serial.print(hum); Serial.println("% -> EXTREME");
        }

        Serial.println("-------------------");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
