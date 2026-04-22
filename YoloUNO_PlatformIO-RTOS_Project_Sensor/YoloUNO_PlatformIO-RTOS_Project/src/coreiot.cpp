#include "coreiot.h"
#include "task_check_info.h"

// ─── Fallback MQTT Config (dùng khi chưa cấu hình qua Web) ──────────────────
static const char* FALLBACK_SERVER = "10.235.76.226";  // ← IP Mosquitto broker
static const char* FALLBACK_TOKEN  = "g7drm1amhd3dchr379xu";
static const int   FALLBACK_PORT   = 1883;

WiFiClient  espClient;
PubSubClient client(espClient);

// ─── MQTT Callback (nhận RPC từ CoreIOT nếu cần) ────────────────────────────
void callback(char* topic, byte* payload, unsigned int length)
{
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    Serial.printf("[MQTT] Topic: %s | Payload: %s\n", topic, message);
}

// ─── Reconnect với token xác thực ────────────────────────────────────────────
void reconnect()
{
    while (!client.connected()) {
        Serial.print("[MQTT] Connecting...");
        String clientId = "ESP32A-" + String(random(0xffff), HEX);

        // Đọc token từ config
        NetConfig_t mqttCfg;
        loadNetConfig(&mqttCfg);
        const char* token = (strlen(mqttCfg.coreToken) == 0) ? FALLBACK_TOKEN : mqttCfg.coreToken;

        if (client.connect(clientId.c_str(), token, NULL)) {
            Serial.println(" connected!");
            client.subscribe("v1/devices/me/rpc/request/+");
            Serial.println("[MQTT] Subscribed: RPC");
        } else {
            Serial.printf("[MQTT] failed (rc=%d), retry in 5s\n", client.state());
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

// ─── CoreIOT Task: đọc Queue → publish telemetry ────────────────────────────
void coreiot_task(void *pvParameters)
{
    // Chờ WiFi kết nối thành công
    Serial.print("[CoreIOT] Waiting for WiFi...");
    if (egWifiStatus != NULL) {
        xEventGroupWaitBits(egWifiStatus, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }
    Serial.println(" WiFi ready!");

    // Đọc cấu hình server
    NetConfig_t cfg;
    loadNetConfig(&cfg);

    const char* server = (strlen(cfg.coreServer) == 0) ? FALLBACK_SERVER : cfg.coreServer;
    int         port   = (cfg.corePort == 0)            ? FALLBACK_PORT  : cfg.corePort;

    client.setServer(server, port);
    client.setCallback(callback);
    Serial.printf("[CoreIOT] Broker: %s:%d\n", server, port);

    while (1) {
        if (!client.connected()) {
            reconnect();
        }
        client.loop();

        // Đọc data cảm biến từ Queue (non-blocking peek)
        SensorData sd = {0.0f, 0.0f};
        if (xQueuePeek(xQueueSensorData, &sd, 0) == pdTRUE) {
            // Publish telemetry lên CoreIOT
            char payload[128];
            snprintf(payload, sizeof(payload),
                     "{\"temperature\":%.1f,\"humidity\":%.1f}", sd.temp, sd.hum);
            client.publish("v1/devices/me/telemetry", payload);
            Serial.printf("[MQTT] Published: %s\n", payload);
        } else {
            Serial.println("[MQTT] No sensor data yet, skipping...");
        }

        vTaskDelay(pdMS_TO_TICKS(10000));  // Publish mỗi 10 giây
    }
}