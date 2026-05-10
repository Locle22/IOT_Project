#include "task_core_iot.h"
#include "global.h"
#include "task_wifi.h"

#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>

constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

constexpr char REMOTE_TEMP_ATTR[] = "remote_temp";
constexpr char REMOTE_HUM_ATTR[] = "remote_hum";
constexpr char LED_STATE_ATTR[] = "ledState";

constexpr std::array<const char *, 3U> SHARED_ATTRIBUTES_LIST = {
    LED_STATE_ATTR,
    REMOTE_TEMP_ATTR,
    REMOTE_HUM_ATTR
};

// Hàm Helper hỗ trợ dọn dẹp và kích hoạt semaphore cho nhiệt độ (3 mức)
static void setSemaphoreByLevel(SemaphoreHandle_t semNormal, SemaphoreHandle_t semWarning, SemaphoreHandle_t semCritical, bool isCritical, bool isWarning) {
    xSemaphoreTake(semCritical, 0); 
    xSemaphoreTake(semWarning, 0); 
    xSemaphoreTake(semNormal, 0);
    if (isCritical) xSemaphoreGive(semCritical);
    else if (isWarning) xSemaphoreGive(semWarning);
    else xSemaphoreGive(semNormal);
}

// Hàm xử lý dữ liệu Shared Attributes nhận từ CoreIOT cloud qua Rule Chain
void processSharedAttributes(const Shared_Attribute_Data &data) {
    SensorData sd = {0.0f, 0.0f};
    xQueuePeek(xQueueSensorData, &sd, 0); // Lấy giá trị hiện tại từ Queue
    bool changed = false;

    for (auto it = data.begin(); it != data.end(); ++it) {
        if (strcmp(it->key().c_str(), REMOTE_TEMP_ATTR) == 0) {
            sd.temp = it->value().as<float>();
            changed = true;
        }
        if (strcmp(it->key().c_str(), REMOTE_HUM_ATTR) == 0) {
            sd.hum = it->value().as<float>();
            changed = true;
        }
        if (strcmp(it->key().c_str(), LED_STATE_ATTR) == 0) {
            bool ledState = it->value().as<bool>();
            Serial.printf("[CoreIOT] Nhận lệnh LED State: %s\n", ledState ? "ON" : "OFF");
        }
    }

    if (changed) {
        // Cập nhật Queue để Task LCD hiển thị dữ liệu mới nhận từ Cloud
        xQueueOverwrite(xQueueSensorData, &sd);

        // --- 1. ĐIỀU KHIỂN SEMAPHORE NHIỆT ĐỘ ---
        bool tempCritical = (sd.temp >= 38.0f);
        bool tempWarning  = (!tempCritical && sd.temp >= 30.0f);
        setSemaphoreByLevel(semTempNormal, semTempWarning, semTempCritical, tempCritical, tempWarning);

        // --- 2. ĐIỀU KHIỂN SEMAPHORE ĐỘ ẨM ---
        xSemaphoreTake(semHum90_100, 0);
        xSemaphoreTake(semHum80_90, 0);
        xSemaphoreTake(semHum50_80, 0);
        xSemaphoreTake(semHum30_50, 0);
        xSemaphoreTake(semHum0_30, 0);

        if (sd.hum >= 90.0f) {
            xSemaphoreGive(semHum90_100);
        } 
        else if (sd.hum >= 80.0f) {
            xSemaphoreGive(semHum80_90);
        } 
        else if (sd.hum >= 50.0f) {
            xSemaphoreGive(semHum50_80);
        } 
        else if (sd.hum >= 30.0f) {
            xSemaphoreGive(semHum30_50);
        } 
        else {
            xSemaphoreGive(semHum0_30);
        }

        Serial.printf("[Server] Cập nhật từ Cloud -> Temp: %.1f, Hum: %.1f\n", sd.temp, sd.hum);
    }
}

// Xử lý các lệnh điều khiển trực tiếp (RPC) từ Dashboard
RPC_Response setLedSwitchValue(const RPC_Data &data) {
    bool newState = data;
    Serial.printf("[RPC] Thay đổi trạng thái Switch: %d\n", newState);
    return RPC_Response("setLedSwitchValue", newState);
}

const std::array<RPC_Callback, 1U> callbacks = {
    RPC_Callback{"setLedSwitchValue", setLedSwitchValue}
};

const Shared_Attribute_Callback attributes_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());
const Attribute_Request_Callback attribute_shared_request_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());

// Hàm quản lý và duy trì kết nối tới máy chủ CoreIOT
void CORE_IOT_reconnect() {
    if (!tb.connected()) {
        NetConfig_t cfg;
        loadNetConfig(&cfg); // Đọc cấu hình từ bộ nhớ Flash ( LittleFS)

        const char* server = (strlen(cfg.coreServer) == 0) ? "app.coreiot.io" : cfg.coreServer;
        const char* token  = cfg.coreToken;
        int port = (cfg.corePort == 0) ? 1883 : cfg.corePort;

        Serial.print("[Server] Đang kết nối CoreIOT Cloud...");
        if (!tb.connect(server, token, port)) {
            Serial.println(" thất bại!");
            return;
        }
        Serial.println(" thành công!");

        // Khai báo thông tin định danh thiết bị
        tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());
        tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());

        // Đăng ký các dịch vụ lắng nghe từ phía máy chủ
        tb.RPC_Subscribe(callbacks.cbegin(), callbacks.cend());
        tb.Shared_Attributes_Subscribe(attributes_callback);
        tb.Shared_Attributes_Request(attribute_shared_request_callback);
    }
}

// Task thực thi chính cho CoreIOT chạy dưới sự quản lý của FreeRTOS
void Task_CoreIOT(void *pvParameters) {
    Serial.println("[Task] Khởi chạy CoreIOT Server...");
    
    // Đảm bảo WiFi ở chế độ STA đã kết nối thành công
    if (egWifiStatus != NULL) {
        xEventGroupWaitBits(egWifiStatus, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }
    
    uint32_t last_ml_send = 0;

    while (1) {
        CORE_IOT_reconnect();

        if (tb.connected()) {
            tb.loop();

            // Định kỳ 5 giây gửi kết quả phân tích AI (TinyML) lên Cloud
            uint32_t now = millis();
            if (now - last_ml_send > 5000) {
                TinyMLMetrics ml_metrics = tinyml_get_metrics();
                tb.sendTelemetryData("ml_result", ml_metrics.predicted_class);
                last_ml_send = now;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}