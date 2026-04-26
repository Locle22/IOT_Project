#include "coreiot.h"
#include "task_check_info.h"
#include <ArduinoJson.h>

// ─── CoreIOT / MQTT Server Config ────────────────────────────────────────────
// QUAN TRỌNG: Điền IP Mosquitto broker (máy Windows của bạn) vào đây
// Sau khi cấu hình qua Web UI thì server/port lấy từ CORE_IOT_SERVER/PORT
const char* coreIOT_Server = "10.235.76.226";   // ← thay bằng IP Mosquitto
const char* coreIOT_Token  = "g7drm1amhd3dchr379xu";
const int   mqttPort       = 1883;

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
        Serial.print("[MQTT] Connecting...");
        String clientId = "ESP32B-" + String(random(0xffff), HEX);
        // Đọc token từ config để xác thực với broker
        NetConfig_t mqttCfg;
        loadNetConfig(&mqttCfg);
        const char* token = (strlen(mqttCfg.coreToken) == 0) ? coreIOT_Token : mqttCfg.coreToken;
        if (client.connect(clientId.c_str(), token, NULL)) {
            Serial.println(" connected!");
            // Subscribe cả RPC lẫn Attribute update
            client.subscribe("v1/devices/me/rpc/request/+");
            client.subscribe("v1/devices/me/attributes");  // Shared Attr
            Serial.println("[MQTT] Subscribed: RPC + Attributes");
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

    // TinyML telemetry timer (send every 5 seconds)
    uint32_t last_telemetry_send = 0;
    const uint32_t TELEMETRY_INTERVAL = 5000; // 5 seconds

    while (1) {
        if (!client.connected()) {
            reconnect();
        }
        client.loop();

        // ── TinyML Telemetry: gửi metrics lên CoreIOT mỗi 5 giây ─────────────
        uint32_t now = millis();
        if (now - last_telemetry_send >= TELEMETRY_INTERVAL) {
            // Lấy dữ liệu sensor
            SensorData sd = {0.0f, 0.0f};
            xQueuePeek(xQueueSensorData, &sd, 0);

            // Lấy dữ liệu TinyML (thread-safe via Mutex)
            TinyMLMetrics ml_metrics = tinyml_get_metrics();

            // Build JSON payload
            StaticJsonDocument<256> doc;
            doc["temp"] = sd.temp;
            doc["hum"] = sd.hum;
            doc["ml_result"] = ml_metrics.predicted_class;  // 0=Background, 1=Fire, 2=Nuisance
            doc["inference_time_us"] = ml_metrics.last_inference_time_us;
            doc["arena_used"] = ml_metrics.arena_used_bytes;

            char jsonBuffer[256];
            serializeJson(doc, jsonBuffer);

            // Publish to telemetry topic
            String topic = "v1/devices/me/telemetry";
            if (client.publish(topic.c_str(), jsonBuffer)) {
                Serial.printf("[TinyML] Telemetry: T=%.1f H=%.1f ML=%d Time=%u Arena=%u\n",
                    sd.temp, sd.hum, ml_metrics.predicted_class,
                    ml_metrics.last_inference_time_us, ml_metrics.arena_used_bytes);
                last_telemetry_send = now;
            } else {
                Serial.println("[TinyML] Telemetry failed");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // Không block lâu — giữ MQTT responsive
    }
}