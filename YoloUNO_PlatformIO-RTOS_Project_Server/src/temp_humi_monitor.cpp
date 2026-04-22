#include "temp_humi_monitor.h"
#include "fan_monitor.h"

// I2C pins của YOLO UNO
#define I2C_SDA 11
#define I2C_SCL 12
// LCD địa chỉ I2C mặc định, 16 cột 2 hàng
#define LCD_ADDR 0x27

/*
 * Task 3: Hiển thị nhiệt độ/độ ẩm lên LCD1602 với 3 trạng thái
 * ──────────────────────────────────────────────────────────────
 *  Dữ liệu đến từ xQueueSensorData (populated bởi coreiot_task khi
 *  nhận Shared Attribute update từ CoreIOT cloud — gốc từ ESP32-A).
 *
 *  NORMAL   (semTempNormal active)   : Hiển thị T và H bình thường
 *  WARNING  (semTempWarning active)  : Hiển thị cảnh báo + giá trị
 *  CRITICAL (semTempCritical active) : Hiển thị nguy hiểm + giá trị
 *
 *  Không dùng biến toàn cục — toàn bộ nhận qua Queue + Semaphore.
 */
void temp_humi_monitor(void *pvParameters)
{
    Wire.begin(I2C_SDA, I2C_SCL);
    LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
    lcd.begin();
    lcd.backlight();

    // Hiển thị khởi động
    lcd.setCursor(0, 0);
    lcd.print("  YOLO UNO IoT  ");
    lcd.setCursor(0, 1);
    lcd.print("  Waiting data..");
    vTaskDelay(pdMS_TO_TICKS(2000));
    lcd.clear();

    SensorData sd = {0.0f, 0.0f};
    bool backlightOn = true;

    while (1) {
        // Block tối đa 8 giây chờ data mới từ CoreIOT
        // Dùng xQueuePeek (không xóa) để WebServer, TinyML cũng đọc được
        bool gotData = (xQueuePeek(xQueueSensorData, &sd, pdMS_TO_TICKS(8000)) == pdTRUE);

        if (!gotData) {
            // Không nhận được data — hiện thông báo mất kết nối
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  No Signal...  ");
            lcd.setCursor(0, 1);
            lcd.print("Check ESP32-A   ");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        lcd.clear();

        // ── Kiểm tra semaphore (non-blocking Take + Give ngay) ──────────────
        if (xSemaphoreTake(semTempCritical, 0) == pdTRUE) {
            // ─────── CRITICAL: temp >= 38°C hoặc hum >= 80% ─────────────────
            lcd.setCursor(0, 0);
            lcd.print("!! NGUY HIEM !! ");
            lcd.setCursor(0, 1);
            char buf[17];
            snprintf(buf, sizeof(buf), "T:%.1fC H:%.0f%%!", sd.temp, sd.hum);
            lcd.print(buf);
            // Bật backlight nháy nhanh — cảnh báo mạnh
            if (!backlightOn) { lcd.backlight(); backlightOn = true; }
            xSemaphoreGive(semTempCritical);   // trả lại cho FanTask và LED

        } else if (xSemaphoreTake(semTempWarning, 0) == pdTRUE) {
            // ─────── WARNING: 30°C <= temp < 38°C hoặc 60% <= hum < 80% ─────
            lcd.setCursor(0, 0);
            lcd.print(" CANH BAO! NHIET");
            lcd.setCursor(0, 1);
            char buf[17];
            snprintf(buf, sizeof(buf), "T:%.1fC H:%.0f%%", sd.temp, sd.hum);
            lcd.print(buf);
            if (!backlightOn) { lcd.backlight(); backlightOn = true; }
            xSemaphoreGive(semTempWarning);

        } else if (xSemaphoreTake(semTempNormal, 0) == pdTRUE) {
            // ─────── NORMAL: temp < 30°C và hum < 60% ────────────────────────
            lcd.setCursor(0, 0);
            char line1[17];
            snprintf(line1, sizeof(line1), "Nhiet do:%.1f'C", sd.temp);
            lcd.print(line1);
            lcd.setCursor(0, 1);
            char line2[17];
            snprintf(line2, sizeof(line2), "Do am:  %.1f %%", sd.hum);
            lcd.print(line2);
            xSemaphoreGive(semTempNormal);

        } else {
            // ─────── Chưa có semaphore nào — fallback hiện raw data ───────────
            lcd.setCursor(0, 0);
            lcd.print("T=");
            lcd.print(sd.temp, 1);
            lcd.print("C");
            lcd.setCursor(0, 1);
            lcd.print("H=");
            lcd.print(sd.hum, 1);
            lcd.print("%");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}