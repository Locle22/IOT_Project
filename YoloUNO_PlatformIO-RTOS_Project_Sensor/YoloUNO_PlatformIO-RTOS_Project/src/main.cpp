#include "global.h"

#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
#include "coreiot.h"

#include "task_check_info.h"
#include "task_wifi.h"
#include "task_webserver.h"

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

     // ── Task: IoT Publishing
    xTaskCreate(Task_CoreIOT,      "Task CoreIOT",  8192, NULL, 2, NULL);

    // ── Task: LED heartbeat (báo hệ thống sống) ─────────────────────────────
    xTaskCreate(led_blinky,        "Task LED",      2048, NULL, 1, NULL);

    // ── Task: NeoPixel heartbeat ─────────────────────────────────────────────
    xTaskCreate(neo_blinky,        "Task NeoPixel", 2048, NULL, 1, NULL);

}

void loop()
{
    // Nếu có config WiFi hợp lệ
    if (check_info_File(1)) {
        if (Wifi_reconnect()) {
            // WiFi đã kết nối → tắt webserver, không cần AP
            Webserver_stop();
        } else {
            // WiFi chưa kết nối → giữ webserver để user cấu hình
            Webserver_reconnect();
        }
    } else {
        // Không có config → chạy webserver AP cho user nhập
        Webserver_reconnect();
    }

    delay(1000);
}