# 🔧 Server (ESP32-B) — Config Backup & Restore Guide

> File này lưu lại config **gốc cho YoloUNO** để phục hồi khi cần.
> Hiện tại đang sửa để chạy trên **ESP32-S3N8R8 (CH340K)**.

---

## 📋 Config gốc — YoloUNO

### `platformio.ini`
```ini
[env:yolo_uno]
platform = espressif32
board = yolo_uno
framework = arduino
monitor_speed = 115200
board_build.filesystem = littlefs

build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -DSSID_AP='"ESP32 LOCAL"'
    -DPASS_AP='12345678'
    -DELEGANTOTA_USE_ASYNC_WEBSERVER=1

lib_deps = 
    tanakamasayuki/TensorFlowLite_ESP32@1.0.0
    adafruit/Adafruit NeoPixel@^1.15.1
    DHT20
    LCD
    PubSubClient
    https://github.com/me-no-dev/ESPAsyncWebServer.git

lib_compat_mode = strict
```

### `include/led_blinky.h`
```cpp
#define LED_GPIO 48
```

### `include/neo_blinky.h`
```cpp
#define NEO_PIN    45
#define LED_COUNT  1
```

### `include/fan_monitor.h`
```cpp
#define FAN_PIN         6
#define PWM_FREQ        25000
#define PWM_CHANNEL     0
#define PWM_RESOLUTION  8
```

### `include/temp_humi_monitor.h`
- Dùng LCD I2C: `LiquidCrystal_I2C.h`
- I2C default của YoloUNO: SDA=11, SCL=12

### `src/coreiot.cpp`
```cpp
const char* coreIOT_Server = "10.235.76.226";
const char* coreIOT_Token  = "g7drm1amhd3dchr379xu";
const int   mqttPort       = 1883;
```

---

## 🔄 Thay đổi cho ESP32-S3N8R8 (CH340K)

| Mục | YoloUNO (gốc) | ESP32-S3N8R8 (hiện tại) |
|-----|---------------|------------------------|
| Board | `yolo_uno` | `esp32-s3-devkitc-1` |
| USB CDC | `ON_BOOT=1, MODE=1` | `ON_BOOT=0` (CH340K) |
| NeoPixel | GPIO 45 | GPIO **48** (onboard) |
| LED đơn | GPIO 48 | GPIO **2** (external) |
| Fan | GPIO 6 | GPIO **6** (giữ nguyên) |
| MQTT Server | `10.235.76.226` | `192.168.1.190` (TinyBroker) |
| MQTT Auth | Token-based | Anonymous |

---

## 🔙 Cách phục hồi về YoloUNO

1. Copy nội dung `platformio.ini` từ phần backup ở trên
2. Đổi lại các pin trong header files:
   - `led_blinky.h`: `LED_GPIO 48`
   - `neo_blinky.h`: `NEO_PIN 45`
3. Đổi lại `coreiot.cpp` về token-based auth
4. Thêm lại folder `boards/yolo_uno.json` nếu đã xóa
5. Xóa `lib_ignore` trong platformio.ini
