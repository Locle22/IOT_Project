#include "led_blinky.h"

/*
 * ESP32-A Heartbeat LED: nhấp nháy chậm báo hệ thống đang sống
 * ─────────────────────────────────────────────────────────────
 *  Bật 200ms → Tắt 2800ms (nhịp thở nhẹ nhàng)
 */
void led_blinky(void *pvParameters)
{
    pinMode(LED_GPIO, OUTPUT);

    while (1) {
        digitalWrite(LED_GPIO, HIGH);
        vTaskDelay(pdMS_TO_TICKS(200));
        digitalWrite(LED_GPIO, LOW);
        vTaskDelay(pdMS_TO_TICKS(2800));
    }
}