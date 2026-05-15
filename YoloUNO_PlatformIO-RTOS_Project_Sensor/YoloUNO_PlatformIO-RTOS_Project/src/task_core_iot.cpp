#include "task_core_iot.h"
#include "global.h"
#include "task_check_info.h"

#include <WiFi.h>            
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, 1024U);

void Task_CoreIOT(void *pvParameters) {
    if (egWifiStatus != NULL) {
        xEventGroupWaitBits(egWifiStatus, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }

    uint32_t last_telemetry_send = 0;
    while (1) {
        if (!tb.connected()) {
            NetConfig_t cfg;
            loadNetConfig(&cfg);
            const char* server = (strlen(cfg.coreServer) == 0) ? "app.coreiot.io" : cfg.coreServer;
            
            tb.connect(server, cfg.coreToken, 1883);
        } 
        else {
            tb.loop();
            uint32_t now = millis();
            if (now - last_telemetry_send >= 5000) { 
                SensorData sd = {0.0f, 0.0f};
                if (xQueuePeek(xQueueSensorData, &sd, 0) == pdTRUE) {
                    tb.sendTelemetryData("temperature", sd.temp);
                    tb.sendTelemetryData("humidity", sd.hum);
                    last_telemetry_send = now;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}