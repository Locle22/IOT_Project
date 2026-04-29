#include "temp_humi_monitor.h"

// I2C pins trên ESP32-S3
#define I2C_SDA 11
#define I2C_SCL 12

/*
 * ESP32-A Sensor Task: Đọc DHT20 → ghi vào Queue
 * ─────────────────────────────────────────────────
 *  Đọc cảm biến mỗi 5 giây.
 *  Dùng xQueueOverwrite để luôn giữ data mới nhất.
 *  CoreIOT task sẽ Peek queue để publish lên MQTT.
 */
void temp_humi_monitor(void *pvParameters)
{
    Wire.begin(I2C_SDA, I2C_SCL);
    DHT20 dht20;
    dht20.begin();

    // Cho sensor ổn định
    vTaskDelay(pdMS_TO_TICKS(2000));
    Serial.println("[DHT20] Sensor initialized.");

    while (1) {
        dht20.read();
        float temperature = dht20.getTemperature();
        float humidity    = dht20.getHumidity();

        if (isnan(temperature) || isnan(humidity)) {
            Serial.println("[DHT20] Read failed!");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        // Ghi vào Queue (overwrite — không block, luôn data mới nhất)
        SensorData sd = { temperature, humidity };
        xQueueOverwrite(xQueueSensorData, &sd);

        Serial.printf("[DHT20] T=%.1f°C  H=%.1f%%\n", temperature, humidity);
        vTaskDelay(pdMS_TO_TICKS(5000));  // Đọc mỗi 5 giây
    }
}