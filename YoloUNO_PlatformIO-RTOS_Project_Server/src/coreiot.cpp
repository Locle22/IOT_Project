#include "coreiot.h"
#include "task_check_info.h"
#include <ArduinoJson.h>

// ─── TinyBroker Config (Server side — port 1884) ─────────────────────────────
// ⚠️ Config gốc YoloUNO đã lưu trong CONFIG_BACKUP.md
const char* coreIOT_Server = "192.168.1.190";  // ← IP máy tính chạy TinyBroker
const char* coreIOT_Token  = "";               // TinyBroker: anonymous
const int   mqttPort       = 1884;             // Port 1884 (Server broker)

WiFiClient  espClient;
PubSubClient client(espClient);

// ─── Helper: cập nhật semaphore theo ngưỡng ──────────────────────────────────
// Drain tất cả → Give đúng 1 semaphore active
static void setSemaphoreByLevel(
    SemaphoreHandle_t semNormal,
    SemaphoreHandle_t semWarning,
    SemaphoreHandle_t semCritical,
    bool isCritical, bool isWarning)
{
    // Drain (đảm bảo không bị đầy)
    xSemaphoreTake(semCritical, 0);
    xSemaphoreTake(semWarning,  0);
    xSemaphoreTake(semNormal,   0);

    if (isCritical)      xSemaphoreGive(semCritical);
    else if (isWarning)  xSemaphoreGive(semWarning);
    else                 xSemaphoreGive(semNormal);
}

// ─── MQTT Callback: nhận RPC + Shared Attribute từ CoreIOT ───────────────────
void callback(char* topic, byte* payload, unsigned int length)
{
    // Allocate buffer
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';

    Serial.printf("[MQTT] Topic: %s | Payload: %s\n", topic, message);

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, message);
    if (err) {
        Serial.printf("[MQTT] JSON parse error: %s\n", err.c_str());
        return;
    }

    // ── Xử lý sensor/data từ TinyGateway: {temperature, humidity} ────────────
    if (strcmp(topic, "sensor/data") == 0) {
        float newTemp = doc["temperature"] | -1.0f;
        float newHum  = doc["humidity"]    | -1.0f;

        if (newTemp < 0 && newHum < 0) return;

        SensorData sd = {0.0f, 0.0f};
        xQueuePeek(xQueueSensorData, &sd, 0);
        if (newTemp >= 0) sd.temp = newTemp;
        if (newHum  >= 0) sd.hum  = newHum;

        xQueueOverwrite(xQueueSensorData, &sd);
        Serial.printf("[Sensor] From Gateway: T=%.1f°C H=%.1f%%\n", sd.temp, sd.hum);

        // Cập nhật semaphore (dùng chung logic với attributes)
        bool tempCritical = (sd.temp >= 38.0f);
        bool tempWarning  = (!tempCritical && sd.temp >= 30.0f);
        setSemaphoreByLevel(semTempNormal, semTempWarning, semTempCritical,
                            tempCritical, tempWarning);

        bool humCritical = (sd.hum >= 80.0f);
        bool humWarning  = (!humCritical && sd.hum >= 60.0f);
        setSemaphoreByLevel(semHumNormal, semHumWarning, semHumCritical,
                            humCritical, humWarning);
        return;
    }
    // ── Xử lý Shared Attribute Update: {remote_temp, remote_hum} ─────────────
    if (strstr(topic, "v1/devices/me/attributes") != nullptr) {
        float newTemp = -1.0f, newHum = -1.0f;

        if (doc.containsKey("remote_temp")) newTemp = doc["remote_temp"].as<float>();
        if (doc.containsKey("remote_hum"))  newHum  = doc["remote_hum"].as<float>();

        // Cần ít nhất 1 giá trị hợp lệ
        if (newTemp < 0 && newHum < 0) return;

        // Nếu chỉ nhận 1 key, peek queue để giữ giá trị kia
        SensorData sd = {0.0f, 0.0f};
        xQueuePeek(xQueueSensorData, &sd, 0);
        if (newTemp >= 0) sd.temp = newTemp;
        if (newHum  >= 0) sd.hum  = newHum;

        // Ghi data mới nhất vào queue (overwrite — không block)
        xQueueOverwrite(xQueueSensorData, &sd);

        // ── Cập nhật semaphore nhiệt độ ──────────────────────────────────────
        bool tempCritical = (sd.temp >= 38.0f);
        bool tempWarning  = (!tempCritical && sd.temp >= 30.0f);
        setSemaphoreByLevel(semTempNormal, semTempWarning, semTempCritical,
                            tempCritical, tempWarning);

        // ── Cập nhật semaphore độ ẩm ─────────────────────────────────────────
        bool humCritical = (sd.hum >= 80.0f);
        bool humWarning  = (!humCritical && sd.hum >= 60.0f);
        setSemaphoreByLevel(semHumNormal, semHumWarning, semHumCritical,
                            humCritical, humWarning);

        Serial.printf("[Sensor] T=%.1f°C H=%.1f%% | TempSem=%s HumSem=%s\n",
            sd.temp, sd.hum,
            tempCritical ? "CRITICAL" : (tempWarning ? "WARNING" : "NORMAL"),
            humCritical  ? "CRITICAL" : (humWarning  ? "WARNING" : "NORMAL"));
        return;
    }

    // ── Xử lý RPC từ CoreIOT (điều khiển từ dashboard) ───────────────────────
    if (strstr(topic, "v1/devices/me/rpc/request/") != nullptr) {
        const char* method = doc["method"];
        if (!method) return;

        if (strcmp(method, "setStateLED") == 0) {
            const char* params = doc["params"];
            if (params) {
                Serial.printf("[RPC] setStateLED → %s\n", params);
                // TODO: gắn thêm logic LED nếu cần
            }
        } else {
            Serial.printf("[RPC] Unknown method: %s\n", method);
        }
    }
}

// ─── Reconnect ───────────────────────────────────────────────────────────────
void reconnect()
{
    while (!client.connected()) {
        Serial.print("[MQTT] Connecting to TinyBroker...");
        String clientId = "ESP32B-" + String(random(0xffff), HEX);

        // TinyBroker: anonymous, không cần token
        if (client.connect(clientId.c_str())) {
            Serial.println(" connected!");
            // Subscribe nhận data cảm biến từ TinyGateway
            client.subscribe("sensor/data");
            client.subscribe("v1/devices/me/attributes");
            Serial.println("[MQTT] Subscribed: sensor/data + attributes");
        } else {
            Serial.printf("[MQTT] failed (rc=%d), retry in 5s\n", client.state());
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

// ─── coreiot_task ─────────────────────────────────────────────────────────────
void coreiot_task(void *pvParameters)
{
    // Chờ WiFi kết nối thành công (EventGroup từ task_wifi)
    Serial.print("[CoreIOT] Waiting for WiFi...");
    if (egWifiStatus != NULL) {
        xEventGroupWaitBits(egWifiStatus, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }
    Serial.println(" WiFi ready!");

    // Đọc cấu hình từ bộ nhớ
    NetConfig_t cfg;
    loadNetConfig(&cfg);

    // Ưu tiên dùng server/port từ file config (nếu có), fallback về hardcode
    const char* server = (strlen(cfg.coreServer) == 0) ? coreIOT_Server : cfg.coreServer;
    int         port   = (cfg.corePort == 0)   ? mqttPort       : cfg.corePort;

    client.setServer(server, port);
    client.setCallback(callback);

    Serial.printf("[CoreIOT] Broker: %s:%d\n", server, port);

    while (1) {
        if (!client.connected()) {
            reconnect();
        }
        client.loop();
        vTaskDelay(pdMS_TO_TICKS(10));  // Không block lâu — giữ MQTT responsive
    }
}