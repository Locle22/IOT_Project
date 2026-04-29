#include "neo_blinky.h"

/*
 * ESP32-A NeoPixel Heartbeat: xanh dương mờ nhấp nháy nhẹ
 * ────────────────────────────────────────────────────────
 *  Báo hiệu hệ thống sensor đang hoạt động
 */
void neo_blinky(void *pvParameters)
{
    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.clear();
    strip.show();

    while (1) {
        // Light green mờ — hít vào
        strip.setPixelColor(0, strip.Color(0, 40, 0));
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(1500));

        // Tắt — thở ra
        strip.clear();
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}