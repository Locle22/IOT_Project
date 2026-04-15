#include <Arduino.h> 
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h> 
#include <ESP32Servo.h> 
#include <ArduinoJson.h> 
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include "DHT20.h"

#include "global_task5.h"
#include "tinyml.h"

// LCD1602
LiquidCrystal_I2C lcd(0x21,16,2);
DHT20 dht20;


// 4 Binary Semaphores cho 4 mức NHIỆT ĐỘ
SemaphoreHandle_t semTempCold;      // < 18°C
SemaphoreHandle_t semTempNormal;    // 18-28°C
SemaphoreHandle_t semTempElevated;  // 28-35°C
SemaphoreHandle_t semTempCritical;  // >= 35°C

// 4 Binary Semaphores cho 4 mức ĐỘ ẨM
SemaphoreHandle_t semHumDry;        // < 30%
SemaphoreHandle_t semHumOptimal;    // 30-60%
SemaphoreHandle_t semHumHumid;      // 60-80%
SemaphoreHandle_t semHumExtreme;    // >= 80%

SemaphoreHandle_t i2cMutex;

void TaskSensorRead(void *pvParameters){
  double temp, hum;
  if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
    dht20.begin();
    xSemaphoreGive(i2cMutex);
  }
  
  while(1){
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
      dht20.read();
      temp = dht20.getTemperature(); 
      hum = dht20.getHumidity();
      xSemaphoreGive(i2cMutex);
    }

    // ae đổi thành switch case 
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

    // Theo độ ẩm 
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

// -- TASK LCD  --
void TaskLCD(void *pvParameters) {
  const char* tempMsg = "";
  const char* humMsg = "";
  double temp = 0, hum = 0;
  
  while(1) {
    // ========== NHIỆT ĐỘ  ==========
    if (xSemaphoreTake(semTempCold, pdMS_TO_TICKS(10)) == pdTRUE) {
      tempMsg = "T:COLD    ";
    } 
    else if (xSemaphoreTake(semTempNormal, pdMS_TO_TICKS(10)) == pdTRUE) {
      tempMsg = "T:NORMAL  ";
    } 
    else if (xSemaphoreTake(semTempElevated, pdMS_TO_TICKS(10)) == pdTRUE) {
      tempMsg = "T:ELEVATED";
    } 
    else if (xSemaphoreTake(semTempCritical, pdMS_TO_TICKS(10)) == pdTRUE) {
      tempMsg = "T:CRITICAL";
    }

    // ==========  ĐỘ ẨM  ==========
    if (xSemaphoreTake(semHumDry, pdMS_TO_TICKS(10)) == pdTRUE) {
      humMsg = "H:DRY     ";
    } 
    else if (xSemaphoreTake(semHumOptimal, pdMS_TO_TICKS(10)) == pdTRUE) {
      humMsg = "H:OPTIMAL ";
    } 
    else if (xSemaphoreTake(semHumHumid, pdMS_TO_TICKS(10)) == pdTRUE) {
      humMsg = "H:HUMID   ";
    } 
    else if (xSemaphoreTake(semHumExtreme, pdMS_TO_TICKS(10)) == pdTRUE) {
      humMsg = "H:EXTREME ";
    }

    // Đọc giá trị hiện tại để hiển thị số
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      dht20.read();
      temp = dht20.getTemperature();
      hum = dht20.getHumidity();
      xSemaphoreGive(i2cMutex);
    }

    // Cập nhật LCD 
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
      lcd.clear();
      
      //  Mức cảnh báo nhiệt độ
      lcd.setCursor(0, 0);
      lcd.print(tempMsg);
      lcd.setCursor(13, 0);
      lcd.print((int)temp);
      lcd.print("C");
      
      //  Mức cảnh báo độ ẩm
      lcd.setCursor(0, 1);
      lcd.print(humMsg);
      lcd.setCursor(13, 1);
      lcd.print((int)hum);
      lcd.print("%");
      
      xSemaphoreGive(i2cMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}


void TaskSensorTinyML(void *pvParameters) {
    SensorData data;
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
        dht20.begin();
        xSemaphoreGive(i2cMutex);
    }

    while (1) {
        if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
            dht20.read();
            data.temperature = dht20.getTemperature();
            data.humidity = dht20.getHumidity();
            xSemaphoreGive(i2cMutex);
        }

        if (sensorQueue != NULL) {
            xQueueOverwrite(sensorQueue, &data);
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}



void setup() {
  Serial.begin(115200);
  Wire.begin(GPIO_NUM_11, GPIO_NUM_12);
  lcd.begin();
  lcd.backlight();  
  lcd.clear();
  lcd.setCursor(0, 0);

  // Nhiệt độ
  semTempCold = xSemaphoreCreateBinary();
  semTempNormal = xSemaphoreCreateBinary();
  semTempElevated = xSemaphoreCreateBinary();
  semTempCritical = xSemaphoreCreateBinary();
  
  // độ ẩm
  semHumDry = xSemaphoreCreateBinary();
  semHumOptimal = xSemaphoreCreateBinary();
  semHumHumid = xSemaphoreCreateBinary();
  semHumExtreme = xSemaphoreCreateBinary();
  
  sensorQueue = xQueueCreate(1, sizeof(SensorData));
  i2cMutex = xSemaphoreCreateMutex();

  // xTaskCreate(TaskSensorRead, "Sensor_Read", 2048, NULL, 1, NULL);
  // xTaskCreate(TaskLCD, "LCD_Display", 2048, NULL, 1, NULL);
  xTaskCreate(TaskSensorTinyML, "Sensor_TinyML", 2048, NULL, 1, NULL);
  xTaskCreate(TaskTinyML, "TinyML", 4096, NULL, 1, NULL);
}

void loop() {

}