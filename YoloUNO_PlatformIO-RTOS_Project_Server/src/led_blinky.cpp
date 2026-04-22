#include "led_blinky.h"

// Helper: blink LED N lần với on/off delay tùy chỉnh
static void blinkPattern(int times, int onMs, int offMs)
{
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_GPIO, HIGH);
        vTaskDelay(pdMS_TO_TICKS(onMs));
        digitalWrite(LED_GPIO, LOW);
        vTaskDelay(pdMS_TO_TICKS(offMs));
    }
}

/*
 * Task 1: LED Blink theo trạng thái nhiệt độ (semaphore-based)
 * ─────────────────────────────────────────────────────────────
 *  NORMAL   (temp < 30°C)  : Nhấp nháy chậm 1 lần — 1s bật / 1s tắt
 *  WARNING  (30–38°C)      : Double-blink 2 lần liên tiếp, dừng 700ms
 *  CRITICAL (temp >= 38°C) : Blink nhanh liên tục 5 lần (100ms) — dạng SOS
 */
void led_blinky(void *pvParameters)
{
    pinMode(LED_GPIO, OUTPUT);

    while (1) {
        if (xSemaphoreTake(semTempCritical, 0) == pdTRUE) {
            // ── CRITICAL: blink nhanh 5 lần (100ms on/off) ──────────────────
            blinkPattern(5, 100, 100);
            xSemaphoreGive(semTempCritical);   // trả lại để task khác đọc
            vTaskDelay(pdMS_TO_TICKS(200));

        } else if (xSemaphoreTake(semTempWarning, 0) == pdTRUE) {
            // ── WARNING: double-blink (300ms on, 150ms off) x2, nghỉ 700ms ──
            blinkPattern(2, 300, 150);
            xSemaphoreGive(semTempWarning);
            vTaskDelay(pdMS_TO_TICKS(700));

        } else if (xSemaphoreTake(semTempNormal, 0) == pdTRUE) {
            // ── NORMAL: nhịp thở chậm 1s/1s ─────────────────────────────────
            blinkPattern(1, 1000, 1000);
            xSemaphoreGive(semTempNormal);

        } else {
            // ── Chưa có dữ liệu: heartbeat nhẹ ──────────────────────────────
            blinkPattern(1, 200, 800);
        }
    }
}