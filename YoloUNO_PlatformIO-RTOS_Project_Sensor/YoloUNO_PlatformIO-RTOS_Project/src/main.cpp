#include "global.h"

#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"

#include "task_check_info.h"
#include "task_wifi.h"
#include "task_webserver.h"
#include "task_core_iot.h"

void setup()
{
    Serial.begin(115200);
    Serial.println("\n═══════════════════════════════════════");
    Serial.println("   ESP32-A  SENSOR NODE  (FreeRTOS)   ");
    Serial.println("═══════════════════════════════════════");

    // Khởi tạo EventGroup trước khi làm gì khác
    egWifiStatus = xEventGroupCreate();

    // Đọc cấu hình WiFi/CoreIOT từ LittleFS
    check_info_File(0);

    // ── Task: Đọc DHT20 → Queue ──────────────────────────────────────────────
    xTaskCreate(temp_humi_monitor, "Task DHT20",    4096, NULL, 3, NULL);

    // ── Task: LED heartbeat (báo hệ thống sống) ─────────────────────────────
    xTaskCreate(led_blinky,        "Task LED",      2048, NULL, 1, NULL);

    // ── Task: NeoPixel heartbeat ─────────────────────────────────────────────
    xTaskCreate(neo_blinky,        "Task NeoPixel", 2048, NULL, 1, NULL);

    // ── Task: IoT Publishing
    xTaskCreate(Task_CoreIOT,      "Task CoreIOT",  8192, NULL, 2, NULL);
}

void loop()
{
    // WiFi reconnect
    if (check_info_File(1)) {
        if (!Wifi_reconnect()) {
            Webserver_stop();
        }
    }
    // Web Server (AP mode — cấu hình WiFi)
    Webserver_reconnect();
}