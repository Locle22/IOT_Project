#include "neo_blinky.h"

/*
 * Task 2: NeoPixel (RGB LED) màu sắc theo trạng thái độ ẩm (semaphore-based)
 * ────────────────────────────────────────────────────────────────────────────
 *  NORMAL   (hum < 60%)  : Xanh lá đều — môi trường khô thoáng
 *  WARNING  (60–80%)     : Cam đều — độ ẩm cao, cần chú ý
 *  CRITICAL (hum >= 80%) : Đỏ nhấp nháy nhanh — nguy hiểm, cần xử lý
 *  Chưa có dữ liệu       : Xanh dương mờ — trạng thái chờ
 */
void neo_blinky(void *pvParameters)
{
    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.clear();
    strip.show();

    while (1) {
        if (xSemaphoreTake(semHumCritical, 0) == pdTRUE) {
            // ── CRITICAL (>= 80%): đỏ nhấp nháy nhanh 200ms ─────────────────
            strip.setPixelColor(0, strip.Color(255, 0, 0));
            strip.show();
            vTaskDelay(pdMS_TO_TICKS(200));
            strip.clear();
            strip.show();
            vTaskDelay(pdMS_TO_TICKS(200));
            xSemaphoreGive(semHumCritical);

        } else if (xSemaphoreTake(semHumWarning, 0) == pdTRUE) {
            // ── WARNING (60–80%): cam đều, nhịp 500ms ────────────────────────
            strip.setPixelColor(0, strip.Color(255, 80, 0));
            strip.show();
            vTaskDelay(pdMS_TO_TICKS(500));
            xSemaphoreGive(semHumWarning);

        } else if (xSemaphoreTake(semHumNormal, 0) == pdTRUE) {
            // ── NORMAL (< 60%): xanh lá đều, nhịp 500ms ─────────────────────
            strip.setPixelColor(0, strip.Color(0, 200, 0));
            strip.show();
            vTaskDelay(pdMS_TO_TICKS(500));
            xSemaphoreGive(semHumNormal);

        } else {
            // ── Chưa có data: xanh dương mờ ──────────────────────────────────
            strip.setPixelColor(0, strip.Color(0, 0, 30));
            strip.show();
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}