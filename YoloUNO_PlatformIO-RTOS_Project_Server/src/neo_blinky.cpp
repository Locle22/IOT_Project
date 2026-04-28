#include "neo_blinky.h"
#include "global.h"

struct RGBColor {
    uint8_t r, g, b;
};

RGBColor getColor(RGBColor start, RGBColor end, int part, int totalParts) {
    float t = (float)part / (totalParts - 1); 
    RGBColor result;
    result.r = (uint8_t)(start.r + (end.r - start.r) * t);
    result.g = (uint8_t)(start.g + (end.g - start.g) * t);
    result.b = (uint8_t)(start.b + (end.b - start.b) * t);
    return result;
}

RGBColor getHumidityColor(float hum) {
    if (hum < 0) hum = 0.0f;
    if (hum > 100) hum = 100.0f;

    RGBColor colorRange[6] = {
        {174, 110, 56}, // 0%
        {172, 128, 52}, // 30%
        {112, 161, 65}, // 50%
        {62, 164, 151}, // 80%
        {56, 128, 175}, // 90%
        {56,  70, 114}  // 100%
    };

    float rangeLimits[6] = {0.0f, 30.0f, 50.0f, 80.0f, 90.0f, 100.0f};
    
    for (int i = 0; i < 5; i++) {
        if (hum >= rangeLimits[i] && hum <= rangeLimits[i+1]) {
            float rangeSize = rangeLimits[i+1] - rangeLimits[i];
            int part = (int)((hum - rangeLimits[i]) / (rangeSize / 5.0f));
            if (part >= 5) part = 4; 
            return getColor(colorRange[i], colorRange[i+1], part, 5);
        }
    }
    return colorRange[5];
}

void neo_blinky(void *pvParameters)
{
    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.clear();
    strip.show();

    SensorData sd;

    while (1) {
        bool isHandled = false;
        
        // ── KHOẢNG 90% - 100% ───────────────────────────────────────────
        if (xSemaphoreTake(semHum90_100, 0) == pdTRUE) {
            if (xQueuePeek(xQueueSensorData, &sd, 0) == pdTRUE) {
                RGBColor c = getHumidityColor(sd.hum);
                strip.setPixelColor(0, strip.Color(c.r, c.g, c.b));
                strip.show();
                vTaskDelay(pdMS_TO_TICKS(200));
                strip.clear();
                strip.show();
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            xSemaphoreGive(semHum90_100);
            isHandled = true;
        } 
        // ── KHOẢNG 80% - 90% ────────────────────────────────────────────
        else if (xSemaphoreTake(semHum80_90, 0) == pdTRUE) {
            if (xQueuePeek(xQueueSensorData, &sd, 0) == pdTRUE) {
                RGBColor c = getHumidityColor(sd.hum);
                strip.setPixelColor(0, strip.Color(c.r, c.g, c.b));
                strip.show();
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            xSemaphoreGive(semHum80_90);
            isHandled = true;
        } 
        // ── KHOẢNG 50% - 80% ────────────────────────────────────────────
        else if (xSemaphoreTake(semHum50_80, 0) == pdTRUE) {
            if (xQueuePeek(xQueueSensorData, &sd, 0) == pdTRUE) {
                RGBColor c = getHumidityColor(sd.hum);
                strip.setPixelColor(0, strip.Color(c.r, c.g, c.b));
                strip.show();
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            xSemaphoreGive(semHum50_80);
            isHandled = true;
        } 
        // ── KHOẢNG 30% - 50% ────────────────────────────────────────────
        else if (xSemaphoreTake(semHum30_50, 0) == pdTRUE) {
            if (xQueuePeek(xQueueSensorData, &sd, 0) == pdTRUE) {
                RGBColor c = getHumidityColor(sd.hum);
                strip.setPixelColor(0, strip.Color(c.r, c.g, c.b));
                strip.show();
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            xSemaphoreGive(semHum30_50);
            isHandled = true;
        } 
        // ── KHOẢNG 0% - 30% ─────────────────────────────────────────────
        else if (xSemaphoreTake(semHum0_30, 0) == pdTRUE) {
            if (xQueuePeek(xQueueSensorData, &sd, 0) == pdTRUE) {
                RGBColor c = getHumidityColor(sd.hum);
                strip.setPixelColor(0, strip.Color(c.r, c.g, c.b));
                strip.show();
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            xSemaphoreGive(semHum0_30);
            isHandled = true;
        }

        // ── FALLBACK NẾU CHƯA CÓ DATA HOẶC MẤT TÍN HIỆU CỜ ───────────────
        if (!isHandled) {
            strip.setPixelColor(0, strip.Color(255, 0, 0));
            strip.show();
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}   