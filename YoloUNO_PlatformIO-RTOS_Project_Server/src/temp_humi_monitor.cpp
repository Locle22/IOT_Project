#include "temp_humi_monitor.h"
#include "fan_monitor.h"

// I2C pins của YOLO UNO
#define I2C_SDA 11
#define I2C_SCL 12
// LCD địa chỉ I2C, 16 cột 2 hàng
#define LCD_ADDR 0x21

// ─── Custom Characters (5x8 pixel) ──────────────────────────────────────────
// Icon nhiệt kế 🌡️
static uint8_t iconTemp[8] = {
    0b00100,
    0b01010,
    0b01010,
    0b01110,
    0b01110,
    0b11111,
    0b11111,
    0b01110
};

// Icon giọt nước 💧
static uint8_t iconDrop[8] = {
    0b00100,
    0b00100,
    0b01010,
    0b01010,
    0b10001,
    0b10001,
    0b10001,
    0b01110
};

// Icon cảnh báo ⚠️
static uint8_t iconWarn[8] = {
    0b00100,
    0b00100,
    0b01010,
    0b01010,
    0b10101,
    0b10001,
    0b10101,
    0b11111
};

// Icon quạt 🌀
static uint8_t iconFan[8] = {
    0b00000,
    0b11001,
    0b01011,
    0b00100,
    0b11010,
    0b10011,
    0b00000,
    0b00000
};

// Icon WiFi 📶
static uint8_t iconWifi[8] = {
    0b00000,
    0b01110,
    0b10001,
    0b00100,
    0b01010,
    0b00000,
    0b00100,
    0b00000
};

#define CHAR_TEMP  0
#define CHAR_DROP  1
#define CHAR_WARN  2
#define CHAR_FAN   3
#define CHAR_WIFI  4

/*
 * Task 3: Hiển thị nhiệt độ/độ ẩm lên LCD1602 với custom icons
 * ──────────────────────────────────────────────────────────────
 *  Dữ liệu đến từ xQueueSensorData (populated bởi coreiot_task khi
 *  nhận sensor/data topic từ TinyBroker — gốc từ ESP32-A).
 *
 *  NORMAL   : 🌡️ T:28.5°C  💧 H:42%
 *  WARNING  : ⚠️ CANH BAO!  + giá trị
 *  CRITICAL : ⚠️⚠️ NGUY HIEM! + giá trị
 */
void temp_humi_monitor(void *pvParameters)
{
    Wire.begin(I2C_SDA, I2C_SCL);

    // ── I2C Scan ──────────────────────────────────────────────────────────────
    Serial.println("[LCD] Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[LCD] Found device at 0x%02X\n", addr);
        }
    }

    LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
    lcd.begin();
    lcd.backlight();

    // ── Tạo custom characters ────────────────────────────────────────────────
    lcd.createChar(CHAR_TEMP, iconTemp);
    lcd.createChar(CHAR_DROP, iconDrop);
    lcd.createChar(CHAR_WARN, iconWarn);
    lcd.createChar(CHAR_FAN,  iconFan);
    lcd.createChar(CHAR_WIFI, iconWifi);
    Serial.printf("[LCD] Initialized at 0x%02X (SDA=%d, SCL=%d)\n", LCD_ADDR, I2C_SDA, I2C_SCL);

    // Hiển thị khởi động
    lcd.setCursor(0, 0);
    lcd.write(CHAR_WIFI);
    lcd.print(" YOLO UNO IoT");
    lcd.setCursor(0, 1);
    lcd.print(" Waiting data...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    lcd.clear();

    SensorData sd = {0.0f, 0.0f};
    bool backlightOn = true;
    bool userLcdOff  = false;

    while (1) {
        // ── Kiểm tra semaphore LCD backlight từ webserver (ưu tiên cao nhất) ──
        if (xSemaphoreTake(semLcdOff, 0) == pdTRUE) {
            if (!userLcdOff) {
                lcd.noBacklight();
                backlightOn = false;
                userLcdOff = true;
                Serial.println("[LCD] Backlight OFF");
            }
            xSemaphoreGive(semLcdOff);
        } else {
            if (userLcdOff) {
                lcd.backlight();
                backlightOn = true;
                userLcdOff = false;
                Serial.println("[LCD] Backlight ON");
            }
        }

        // Block tối đa 8 giây chờ data mới
        bool gotData = (xQueuePeek(xQueueSensorData, &sd, pdMS_TO_TICKS(8000)) == pdTRUE);

        if (!gotData) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.write(CHAR_WARN);
            lcd.print(" No Signal!    ");
            lcd.setCursor(0, 1);
            lcd.print(" Check ESP32-A ");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        lcd.clear();

        // ── Trạng thái quạt (hiển thị góc phải) ─────────────────────────────
        bool fanOn = FanGetState();

        // ── Kiểm tra semaphore nhiệt độ ─────────────────────────────────────
        if (xSemaphoreTake(semTempCritical, 0) == pdTRUE) {
            // ─── CRITICAL ────────────────────────────────────────────────────
            lcd.setCursor(0, 0);
            lcd.write(CHAR_WARN);
            lcd.write(CHAR_WARN);
            lcd.print(" NGUY HIEM!");
            if (fanOn) { lcd.setCursor(15, 0); lcd.write(CHAR_FAN); }

            lcd.setCursor(0, 1);
            lcd.write(CHAR_TEMP);
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", sd.temp);
            lcd.print(buf);
            lcd.print("\xDF" "C ");
            lcd.write(CHAR_DROP);
            snprintf(buf, sizeof(buf), "%.0f%%", sd.hum);
            lcd.print(buf);
            xSemaphoreGive(semTempCritical);

        } else if (xSemaphoreTake(semTempWarning, 0) == pdTRUE) {
            // ─── WARNING ─────────────────────────────────────────────────────
            lcd.setCursor(0, 0);
            lcd.write(CHAR_WARN);
            lcd.print(" CANH BAO!  ");
            if (fanOn) { lcd.setCursor(15, 0); lcd.write(CHAR_FAN); }

            lcd.setCursor(0, 1);
            lcd.write(CHAR_TEMP);
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", sd.temp);
            lcd.print(buf);
            lcd.print("\xDF" "C ");
            lcd.write(CHAR_DROP);
            snprintf(buf, sizeof(buf), "%.0f%%", sd.hum);
            lcd.print(buf);
            xSemaphoreGive(semTempWarning);

        } else if (xSemaphoreTake(semTempNormal, 0) == pdTRUE) {
            // ─── NORMAL ──────────────────────────────────────────────────────
            lcd.setCursor(0, 0);
            lcd.write(CHAR_TEMP);
            char line1[16];
            snprintf(line1, sizeof(line1), " Temp: %.1f", sd.temp);
            lcd.print(line1);
            lcd.print("\xDF" "C");

            lcd.setCursor(0, 1);
            lcd.write(CHAR_DROP);
            char line2[16];
            snprintf(line2, sizeof(line2), " Humi: %.1f%%", sd.hum);
            lcd.print(line2);
            xSemaphoreGive(semTempNormal);

        } else {
            // ─── Fallback ────────────────────────────────────────────────────
            lcd.setCursor(0, 0);
            lcd.write(CHAR_TEMP);
            lcd.print(" T=");
            lcd.print(sd.temp, 1);
            lcd.print("\xDF" "C");
            lcd.setCursor(0, 1);
            lcd.write(CHAR_DROP);
            lcd.print(" H=");
            lcd.print(sd.hum, 1);
            lcd.print("%");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}