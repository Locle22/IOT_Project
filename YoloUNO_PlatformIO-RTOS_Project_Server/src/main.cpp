#include "global.h"

#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
#include "fan_monitor.h"
#include "coreiot.h"

// include tasks
#include "task_check_info.h"
#include "task_wifi.h"
#include "task_webserver.h"

void setup()
{
    Serial.begin(115200);
    // Khởi tạo EventGroup hệ thống wifi trước khi làm gì khác
    egWifiStatus = xEventGroupCreate();

    // Khởi động LittleFS và nạp cấu hình WiFi/CoreIOT đã lưu
    check_info_File(0);

    // ── Task 1: LED blink theo trạng thái nhiệt độ ────────────────────────────
    xTaskCreate(led_blinky,         "Task LED Blink",      2048, NULL, 2, NULL);

    // ── Task 2: NeoPixel màu theo trạng thái độ ẩm ───────────────────────────
    xTaskCreate(neo_blinky,         "Task NEO Blink",      2048, NULL, 2, NULL);

    // ── Task 3: LCD hiển thị dữ liệu từ Queue + Semaphore ────────────────────
    xTaskCreate(temp_humi_monitor,  "Task LCD Monitor",    4096, NULL, 3, NULL);

    // ── Fan Control: auto theo semaphore nhiệt độ, manual từ Web ─────────────
    xTaskCreate(FanControlTask,     "Task Fan Control",    2048, NULL, 2, NULL);

    // ── CoreIOT: nhận Shared Attributes từ Mosquitto → CoreIOT → ESP32-B ─────
    xTaskCreate(coreiot_task,       "Task CoreIOT",        8192, NULL, 2, NULL);
}

void loop()
{
    // WiFi reconnect logic
    if (check_info_File(1)) {
        if (!Wifi_reconnect()) {
            Webserver_stop();
        }
    }
    // Web Server reconnect (AP mode luôn sẵn sàng)
    Webserver_reconnect();
}