#include <Arduino.h> 
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h> 
#include <ESP32Servo.h> 
#include <ArduinoJson.h> 
#include <SPI.h>
#include <Wire.h>
#include "SensorTask.h"
#include "FanMonitor.h"
#include "LCDTask.h"
#include "WebServerTask.h"

// AP Configuration - ESP32 sẽ phát WiFi với tên/mật khẩu bên dưới
#define WIFI_SSID "ESP32_WEB"
#define WIFI_PASSWORD "12345678"

void setup() {
  Serial.begin(115200);
  Wire.begin(GPIO_NUM_11, GPIO_NUM_12);
  
  // Khởi tạo Semaphores
  initSensorSemaphores(); 
  
  // Khởi tạo LCD
  initLCD();
  FanInit();
  // Khởi tạo Web Server (ESP32 phát WiFi AP)
  initWebServer(WIFI_SSID, WIFI_PASSWORD);

  // Tạo Tasks
  xTaskCreate(TaskSensorRead, "Sensor_Read", 2048, NULL, 1, NULL);
  xTaskCreate(TaskLCD, "LCD_Display", 2048, NULL, 1, NULL);
  xTaskCreate(TaskWebServer, "Web_Server", 8192, NULL, 1, NULL);
  xTaskCreate(FanControlTask, "Fan_Control", 2048, NULL, 1, NULL);
}

void loop() {

}