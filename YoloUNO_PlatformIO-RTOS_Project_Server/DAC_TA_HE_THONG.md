# 📖 ĐẶC TẢ HỆ THỐNG DUAL-ESP32 IoT — FreeRTOS

> **Phiên bản:** 1.0 · **Ngày:** 22/04/2026  
> **Môn học:** Assignment IoT — RTOS  
> **Tác giả:** Nhóm phát triển

---

## Mục Lục

1. [Tổng Quan Hệ Thống](#1-tổng-quan-hệ-thống)
2. [Kiến Trúc Phần Mềm](#2-kiến-trúc-phần-mềm)
3. [Thiết Lập Hạ Tầng (Broker + CoreIOT)](#3-thiết-lập-hạ-tầng-broker--coreiot)
4. [ESP32-A: Sensor Node](#4-esp32-a-sensor-node)
5. [ESP32-B: Controller Node](#5-esp32-b-controller-node)
6. [Luồng Dữ Liệu End-to-End](#6-luồng-dữ-liệu-end-to-end)
7. [Cơ Chế RTOS Chi Tiết](#7-cơ-chế-rtos-chi-tiết)
8. [Chế Độ Hoạt Động & Chuyển Đổi](#8-chế-độ-hoạt-động--chuyển-đổi)
9. [API Web Server](#9-api-web-server)
10. [Cấu Trúc Thư Mục](#10-cấu-trúc-thư-mục)
11. [Hướng Dẫn Triển Khai Từng Bước](#11-hướng-dẫn-triển-khai-từng-bước)
12. [Troubleshooting](#12-troubleshooting)

---

## 1. Tổng Quan Hệ Thống

### 1.1 Mô tả

Hệ thống IoT gồm **2 vi điều khiển ESP32** (board YoloUNO — ESP32-S3) giao tiếp với nhau thông qua **Cloud (CoreIOT)** theo mô hình Publisher-Subscriber:

| Thành phần | Vai trò | 
|---|---|
| **ESP32-A** (Sensor Node) | Đọc cảm biến DHT20, publish telemetry lên MQTT broker |
| **ESP32-B** (Controller Node) | Nhận dữ liệu qua CoreIOT Shared Attributes, điều khiển LCD/LED/Quạt |
| **Mosquitto Broker** | Trung gian MQTT chạy trên máy tính cá nhân |
| **CoreIOT Server** | Cloud platform (app.coreiot.io), tạo Rule Chain chuyển tiếp data |

### 1.2 Sơ Đồ Tổng Thể

```
┌─────────────────┐                    ┌──────────────────┐
│   ESP32-A        │    WiFi + MQTT     │   Mosquitto      │
│   (Sensor Node)  │──── Publish ──────▶│   MQTT Broker    │
│                  │  "v1/devices/me    │   (máy tính)     │
│   📡 DHT20      │   /telemetry"      │   Port: 1883     │
│   💡 LED (HB)   │                    └───────┬──────────┘
│   🔵 NeoPixel   │                            │
└─────────────────┘                   MQTT forward (hoặc trực tiếp)
                                                │
                                                ▼
                                       ┌──────────────────┐
                                       │   CoreIOT Server │
                                       │  app.coreiot.io  │
                                       │                  │
                                       │  • Device A      │
                                       │  • Device B      │
                                       │  • Rule Chain    │
                                       └───────┬──────────┘
                                                │
                                     Shared Attribute Update
                                     {remote_temp, remote_hum}
                                                │
                                                ▼
                                       ┌──────────────────┐
                                       │   ESP32-B         │
                                       │   (Controller)    │
                                       │                   │
                                       │   📺 LCD 1602     │
                                       │   💡 LED (Temp)   │
                                       │   🔴 NeoPixel(Hum)│
                                       │   🌀 Quạt PWM     │
                                       │   🌐 Web Dashboard│
                                       └──────────────────┘
```

---

## 2. Kiến Trúc Phần Mềm

### 2.1 Nguyên Tắc Thiết Kế

| Nguyên tắc | Mô tả |
|---|---|
| **Zero Global Variables** | Không dùng biến toàn cục cho dữ liệu sensor hay cấu hình mạng. Mọi giao tiếp đều qua Queue, Semaphore, EventGroup |
| **Struct Configuration** | Cấu hình WiFi/MQTT được lưu trong `NetConfig_t` struct, đọc từ LittleFS khi cần |
| **Event-Driven** | Các task không polling liên tục, mà chờ tín hiệu qua Semaphore hoặc EventGroup |
| **Producer-Consumer** | DHT20 task là Producer (ghi Queue), các task khác là Consumer (đọc Queue) |

### 2.2 Các Cơ Chế FreeRTOS Sử Dụng

```
┌─────────────────────────────────────────────────────────────┐
│                    FreeRTOS Primitives                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  xQueueSensorData (Queue size=1)                            │
│    ├── Producer: DHT20 Task (xQueueOverwrite)               │
│    └── Consumer: LCD Task, WebServer, TinyML (xQueuePeek)   │
│                                                             │
│  semTemp{Normal|Warning|Critical} (Binary Semaphore x3)     │
│    ├── Producer: MQTT Callback (setSemaphoreByLevel)         │
│    └── Consumer: LED Task, LCD Task, Fan Task                │
│                                                             │
│  semHum{Normal|Warning|Critical} (Binary Semaphore x3)      │
│    ├── Producer: MQTT Callback (setSemaphoreByLevel)         │
│    └── Consumer: NeoPixel Task                               │
│                                                             │
│  egWifiStatus (EventGroup)                                  │
│    ├── Setter: WiFi Task (set/clear WIFI_CONNECTED_BIT)     │
│    └── Waiter: CoreIOT Task (xEventGroupWaitBits)           │
│                                                             │
│  NetConfig_t (Local Struct)                                 │
│    ├── Source: LittleFS → /info.dat (JSON file)             │
│    └── Reader: loadNetConfig(&cfg) — đọc khi cần, giải     │
│               phóng sau khi hàm kết thúc                    │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Thiết Lập Hạ Tầng (Broker + CoreIOT)

### 3.1 Cài Đặt Mosquitto MQTT Broker (trên máy tính)

#### Bước 1: Cài đặt
- **Windows:** Tải từ https://mosquitto.org/download/ → Cài đặt
- **macOS:** `brew install mosquitto`
- **Linux:** `sudo apt install mosquitto mosquitto-clients`

#### Bước 2: Cấu hình cho phép kết nối từ xa
Mở file `mosquitto.conf` (thường ở `C:\Program Files\mosquitto\mosquitto.conf`):
```conf
# Cho phép kết nối không cần mật khẩu (dùng cho test)
allow_anonymous true

# Lắng nghe trên tất cả interface
listener 1883 0.0.0.0
```

#### Bước 3: Khởi động
```bash
# Windows (PowerShell Admin)
net start mosquitto

# Hoặc chạy thủ công
mosquitto -c "C:\Program Files\mosquitto\mosquitto.conf" -v
```

#### Bước 4: Test bằng dòng lệnh
```bash
# Terminal 1: Subscribe (lắng nghe)
mosquitto_sub -h localhost -t "v1/devices/me/telemetry" -v

# Terminal 2: Publish (gửi thử)
mosquitto_pub -h localhost -t "v1/devices/me/telemetry" \
  -m '{"temperature":28.5,"humidity":65}'
```

#### Bước 5: Xác định IP máy tính
```bash
# Windows
ipconfig    # Tìm dòng "IPv4 Address" của WiFi adapter

# Ví dụ: 192.168.1.100 — đây là giá trị sẽ nhập vào "Máy chủ" khi cấu hình ESP32
```

### 3.2 Thiết Lập CoreIOT (app.coreiot.io)

#### Bước 1: Tạo 2 Devices
1. Đăng nhập https://app.coreiot.io
2. Vào **Devices** → **Add New Device**
3. Tạo 2 device:
   - `ESP32-A-Sensor` → Copy **Access Token** (ví dụ: `abc123sensor`)
   - `ESP32-B-Controller` → Copy **Access Token** (ví dụ: `xyz789ctrl`)

#### Bước 2: Tạo Rule Chain chuyển tiếp dữ liệu
Mục đích: Khi ESP32-A gửi telemetry `{temperature, humidity}` → CoreIOT tự động chuyển thành **Shared Attributes** `{remote_temp, remote_hum}` trên device ESP32-B.

1. Vào **Rule Chains** → Mở **Root Rule Chain**
2. Thêm node mới:

```
[Message Type Switch]
    │
    ├── Post Telemetry ──▶ [Script Transformation]
    │                        │
    │                        │ Trong script:
    │                        │   var newMsg = {};
    │                        │   newMsg.remote_temp = msg.temperature;
    │                        │   newMsg.remote_hum = msg.humidity;
    │                        │   return {msg: newMsg, metadata: metadata, msgType: msgType};
    │                        │
    │                        ▼
    │                   [Change Originator]
    │                     Type: Device
    │                     Name: ESP32-B-Controller
    │                        │
    │                        ▼
    │                   [Save Attributes]
    │                     Scope: Shared
    │
    └── Other ──▶ ...
```

3. Kết nối các node và **Save** Rule Chain

#### Bước 3: Verify
- Gửi telemetry thử từ ESP32-A (hoặc REST API)
- Kiểm tra tab **Shared Attributes** của device ESP32-B trên CoreIOT:
  - Xuất hiện `remote_temp` = giá trị nhiệt độ
  - Xuất hiện `remote_hum` = giá trị độ ẩm

---

## 4. ESP32-A: Sensor Node

### 4.1 Chức Năng
ESP32-A là node đọc cảm biến, **chỉ publish** data lên cloud, không điều khiển thiết bị.

### 4.2 Danh Sách Tasks

```
┌─────────────────────────────────────────────────────────┐
│  main.cpp → setup()                                     │
│                                                         │
│  ┌──────────────────────┐  Priority 3, Stack 4096       │
│  │ temp_humi_monitor    │  Đọc DHT20 mỗi 5s            │
│  │ (DHT20 Task)         │──▶ xQueueOverwrite            │
│  └──────────────────────┘                               │
│                                                         │
│  ┌──────────────────────┐  Priority 2, Stack 8192       │
│  │ coreiot_task         │  Peek Queue mỗi 10s           │
│  │ (MQTT Publish Task)  │──▶ client.publish(telemetry)  │
│  └──────────────────────┘                               │
│                                                         │
│  ┌──────────────────────┐  Priority 1, Stack 2048       │
│  │ led_blinky           │  Heartbeat: 200ms ON          │
│  │ (LED Heartbeat)      │            2800ms OFF         │
│  └──────────────────────┘                               │
│                                                         │
│  ┌──────────────────────┐  Priority 1, Stack 2048       │
│  │ neo_blinky            │  Xanh dương mờ               │
│  │ (NeoPixel Heartbeat) │  1500ms ON / 1500ms OFF       │
│  └──────────────────────┘                               │
│                                                         │
│  loop() → WiFi reconnect + WebServer AP                 │
└─────────────────────────────────────────────────────────┘
```

### 4.3 Luồng Hoạt Động

```
                    ┌──────────┐
                    │  DHT20   │
                    │  Sensor  │
                    └────┬─────┘
                         │ dht20.read()
                         ▼
              ┌────────────────────┐
              │ temp_humi_monitor  │
              │ Task (5s interval) │
              └────────┬───────────┘
                       │ xQueueOverwrite
                       ▼
              ┌────────────────────┐
              │  xQueueSensorData  │  ← Queue size=1
              │  {temp, hum}       │     luôn giữ data mới nhất
              └────────┬───────────┘
                       │ xQueuePeek
                       ▼
              ┌────────────────────┐
              │  coreiot_task      │
              │  (10s interval)    │
              └────────┬───────────┘
                       │ client.publish
                       ▼
              ┌─────────────────────────────────────────┐
              │  MQTT Topic: v1/devices/me/telemetry    │
              │  Payload: {"temperature":28.5,           │
              │            "humidity":65.2}               │
              └─────────────────────────────────────────┘
```

### 4.4 File Quan Trọng Cần Sửa Khi Thay Đổi

| Nhu cầu | File cần sửa | Vị trí |
|---|---|---|
| Đổi IP broker MQTT | `coreiot.cpp` | Dòng `FALLBACK_SERVER` |
| Đổi token device | `coreiot.cpp` | Dòng `FALLBACK_TOKEN` |
| Đổi tần suất đọc sensor | `temp_humi_monitor.cpp` | `vTaskDelay(pdMS_TO_TICKS(5000))` |
| Đổi tần suất publish | `coreiot.cpp` | `vTaskDelay(pdMS_TO_TICKS(10000))` |
| Đổi tên AP WiFi | `platformio.ini` | `-DSSID_AP='"..."'` |
| Đổi chân I2C | `temp_humi_monitor.cpp` | `I2C_SDA`, `I2C_SCL` |

---

## 5. ESP32-B: Controller Node

### 5.1 Chức Năng
ESP32-B nhận data cảm biến từ cloud qua MQTT Shared Attributes, rồi điều khiển 4 thiết bị đầu ra + cung cấp Web Dashboard.

### 5.2 Danh Sách Tasks

```
┌─────────────────────────────────────────────────────────┐
│  main.cpp → setup()                                     │
│                                                         │
│  ┌──────────────────────┐  Priority 3, Stack 4096       │
│  │ temp_humi_monitor    │  Peek Queue → Hiển thị LCD    │
│  │ (Task 3 — LCD)       │  Trạng thái theo Semaphore    │
│  └──────────────────────┘                               │
│                                                         │
│  ┌──────────────────────┐  Priority 2, Stack 8192       │
│  │ coreiot_task         │  Subscribe MQTT Attributes    │
│  │ (Task MQTT Subscribe)│  Callback → Queue + Semaphore │
│  └──────────────────────┘                               │
│                                                         │
│  ┌──────────────────────┐  Priority 2, Stack 2048       │
│  │ led_blinky           │  Task 1 — LED theo nhiệt độ   │
│  │ (Temp LED Task)      │  Normal/Warning/Critical      │
│  └──────────────────────┘                               │
│                                                         │
│  ┌──────────────────────┐  Priority 2, Stack 2048       │
│  │ neo_blinky           │  Task 2 — NeoPixel theo ẩm    │
│  │ (Humidity RGB Task)  │  Xanh/Cam/Đỏ nhấp nháy       │
│  └──────────────────────┘                               │
│                                                         │
│  ┌──────────────────────┐  Priority 2, Stack 2048       │
│  │ FanControlTask       │  Quạt Auto/Manual             │
│  │ (Fan Control)        │  Theo semaphore hoặc Web      │
│  └──────────────────────┘                               │
│                                                         │
│  loop() → WiFi reconnect + WebServer AP                 │
└─────────────────────────────────────────────────────────┘
```

### 5.3 Bảng Ngưỡng Cảm Biến

| Trạng thái | Nhiệt độ | Độ ẩm | LED (GPIO 48) | NeoPixel (GPIO 45) | LCD | Quạt (GPIO 6) |
|:---:|---|---|---|---|---|---|
| 🟢 NORMAL | < 30°C | < 60% | Chậm 1s/1s | Xanh lá đều | Hiện giá trị | TẮT (Auto) |
| 🟡 WARNING | 30–38°C | 60–80% | Double-blink | Cam đều | "CANH BAO!" | BẬT (Auto) |
| 🔴 CRITICAL | ≥ 38°C | ≥ 80% | SOS nhanh x5 | Đỏ nhấp nháy | "!! NGUY HIEM !!" | BẬT (Auto) |

### 5.4 File Quan Trọng Cần Sửa

| Nhu cầu | File cần sửa | Vị trí |
|---|---|---|
| Đổi IP broker MQTT | `coreiot.cpp` | Dòng `coreIOT_Server` |
| Đổi token device B | `coreiot.cpp` | Dòng `coreIOT_Token` |
| Đổi ngưỡng nhiệt độ | `coreiot.cpp` | Callback → `tempCritical`, `tempWarning` |
| Đổi ngưỡng độ ẩm | `coreiot.cpp` | Callback → `humCritical`, `humWarning` |
| Đổi tên AP WiFi | `platformio.ini` | `-DSSID_AP='"..."'` |
| Đổi chân GPIO quạt | `include/fan_monitor.h` | `#define FAN_PIN` |

---

## 6. Luồng Dữ Liệu End-to-End

```
Bước 1: ESP32-A đọc DHT20
   │  temp_humi_monitor.cpp
   │  dht20.read() → SensorData{28.5, 65.2}
   │  xQueueOverwrite(xQueueSensorData, &sd)
   │
Bước 2: ESP32-A publish MQTT
   │  coreiot.cpp → coreiot_task()
   │  xQueuePeek(xQueueSensorData) → sd
   │  client.publish("v1/devices/me/telemetry",
   │                 '{"temperature":28.5,"humidity":65.2}')
   │
   │  ──────── WiFi ────────▶ Mosquitto Broker (192.168.x.x:1883)
   │
Bước 3: CoreIOT nhận telemetry
   │  Mosquitto forward → CoreIOT (app.coreiot.io)
   │  Device ESP32-A-Sensor nhận telemetry
   │
Bước 4: Rule Chain chuyển tiếp
   │  Script Transform: temperature → remote_temp
   │                    humidity → remote_hum
   │  Change Originator → ESP32-B-Controller
   │  Save Attributes (Shared)
   │
Bước 5: ESP32-B nhận Shared Attribute
   │  coreiot.cpp → callback()
   │  Topic: "v1/devices/me/attributes"
   │  Payload: {"remote_temp":28.5, "remote_hum":65.2}
   │
Bước 6: ESP32-B cập nhật Queue + Semaphore
   │  callback():
   │    xQueueOverwrite(xQueueSensorData, &sd)
   │    setSemaphoreByLevel(semTemp...) → NORMAL
   │    setSemaphoreByLevel(semHum...)  → WARNING
   │
Bước 7: Các Task phản ứng
   │  LED Task:       xSemaphoreTake(semTempNormal) → nhấp nháy chậm
   │  NeoPixel Task:  xSemaphoreTake(semHumWarning) → cam đều
   │  LCD Task:       xQueuePeek → hiện "T:28.5C H:65.2%"
   │  Fan Task:       xSemaphoreTake(semTempNormal) → tắt quạt
   │  Web Dashboard:  /sensor API → JSON {"temp":28.5,"hum":65.2}
```

---

## 7. Cơ Chế RTOS Chi Tiết

### 7.1 Queue (xQueueSensorData)

```c
// Khai báo (global.cpp)
QueueHandle_t xQueueSensorData = xQueueCreate(1, sizeof(SensorData));

// Producer ghi (ESP32-A: DHT20 Task, ESP32-B: MQTT Callback)
xQueueOverwrite(xQueueSensorData, &sd);   // Overwrite — luôn giữ data MỚI NHẤT

// Consumer đọc (LCD, WebServer, TinyML)
xQueuePeek(xQueueSensorData, &sd, 0);     // Peek — ĐỌC mà KHÔNG XÓA
```

> **Tại sao Peek mà không Receive?**  
> Vì nhiều task cùng cần đọc cùng 1 data. `xQueueReceive` sẽ lấy data ra khỏi queue → các task khác đọc được 0.

### 7.2 Semaphore (Binary — cho trạng thái)

```c
// Chỉ có 1 trong 3 semaphore được Give tại một thời điểm
// Ví dụ: khi temp = 35°C (WARNING)
xSemaphoreTake(semTempCritical, 0);  // Drain
xSemaphoreTake(semTempWarning,  0);  // Drain
xSemaphoreTake(semTempNormal,   0);  // Drain
xSemaphoreGive(semTempWarning);      // ← Active duy nhất

// Consumer kiểm tra:
if (xSemaphoreTake(semTempWarning, 0) == pdTRUE) {
    // Đang ở trạng thái WARNING
    xSemaphoreGive(semTempWarning);  // Trả lại cho task khác
}
```

### 7.3 EventGroup (egWifiStatus)

```c
// WiFi Task: khi kết nối thành công
xEventGroupSetBits(egWifiStatus, WIFI_CONNECTED_BIT);

// CoreIOT Task: chờ WiFi trước khi connect MQTT
xEventGroupWaitBits(egWifiStatus, WIFI_CONNECTED_BIT,
                    pdFALSE,    // Không xóa bit sau khi đọc
                    pdTRUE,     // Chờ tất cả bits
                    portMAX_DELAY);  // Block vô hạn
```

### 7.4 NetConfig_t (Struct cấu hình)

```c
// Đọc cấu hình từ LittleFS khi cần, không giữ toàn cục
NetConfig_t cfg;
loadNetConfig(&cfg);
WiFi.begin(cfg.ssid, cfg.pass);
// cfg tự hủy khi hàm kết thúc → không tốn RAM liên tục
```

---

## 8. Chế Độ Hoạt Động & Chuyển Đổi

### 8.1 Sơ Đồ Chế Độ

```
┌────────────────────────────────────┐
│          POWER ON                  │
└──────────────┬─────────────────────┘
               │
               ▼
     ┌─────────────────────┐
     │  Đọc /info.dat      │
     │  từ LittleFS        │
     └─────────┬───────────┘
               │
        ┌──────┴──────┐
        │ Có WiFi     │ Không có / File rỗng
        │ config?     │
        ├─── CÓ ──┐  ├─── KHÔNG ──┐
        │          │  │            │
        ▼          │  ▼            │
  ┌──────────┐     │  ┌──────────────┐
  │ STA Mode │     │  │ AP Mode      │
  │ Kết nối  │     │  │ SSID: ESP32  │
  │ WiFi nhà │     │  │ IP: 192.168  │
  │          │     │  │      .4.1    │
  └────┬─────┘     │  └──────┬───────┘
       │           │         │
       ▼           │         ▼
  ┌──────────┐     │  ┌─────────────────┐
  │ MQTT     │     │  │ Web Server      │
  │ Connect  │     │  │ Hiện form config│
  │ CoreIOT  │     │  │ /connect API    │
  └──────────┘     │  └───────┬─────────┘
                   │          │ User nhập SSID/Pass
                   │          │ → Save_info_File()
                   │          │ → ESP.restart()
                   │          │
                   │  ┌───────┘
                   │  │
                   ▼  ▼
             ┌─────────────┐
             │  Reboot     │
             │  (lặp lại   │
             │   từ đầu)   │
             └─────────────┘
```

### 8.2 Chế Độ Quạt (ESP32-B)

```
                ┌────────────────────┐
                │   FanControlTask   │
                └────────┬───────────┘
                         │
                  ┌──────┴──────┐
                  │ Manual      │
                  │ Override?   │
                  ├── CÓ ──┐   ├── KHÔNG ──┐
                  │         │   │           │
                  ▼         │   ▼           │
           ┌──────────┐    │   ┌──────────────┐
           │ Web      │    │   │ Tự động      │
           │ Command  │    │   │ theo         │
           │ ON/OFF   │    │   │ Semaphore    │
           └──────────┘    │   └──────────────┘
                           │
         Cách bật Manual:  │   Cách trả về Auto:
         /action?dev=fan   │   /action?dev=fan
              &state=ON    │        &state=AUTO
```

---

## 9. API Web Server

### ESP32-A (AP: `ESP32-A SENSOR`, IP: `192.168.4.1`)

| Endpoint | Method | Mô tả | Ví dụ |
|---|---|---|---|
| `/` | GET | Trang cấu hình HTML | — |
| `/connect` | GET | Lưu config WiFi + CoreIOT | `/connect?ssid=MyWifi&pass=123&token=abc&server=app.coreiot.io&port=1883` |
| `/sensor` | GET | Xem data sensor hiện tại | Response: `{"temp":28.5,"hum":65.2}` |

### ESP32-B (AP: `ESP32 LOCAL`, IP: `192.168.4.1`)

| Endpoint | Method | Mô tả | Ví dụ |
|---|---|---|---|
| `/` | GET | Dashboard HTML + WebSocket | — |
| `/connect` | GET | Lưu config WiFi + CoreIOT | Tương tự ESP32-A |
| `/sensor` | GET | Xem data sensor mới nhất | Response: `{"temp":28.5,"hum":65.2}` |
| `/action` | GET | Điều khiển thiết bị | Xem bảng dưới |

#### Chi tiết `/action`:

| Lệnh | URL |
|---|---|
| Bật quạt | `/action?dev=fan&state=ON` |
| Tắt quạt | `/action?dev=fan&state=OFF` |
| Quạt tự động | `/action?dev=fan&state=AUTO` |
| Đặt tốc độ quạt | `/action?dev=fan&state=SPEED&value=180` |
| Bật LCD | `/action?dev=lcd&state=ON` |

---

## 10. Cấu Trúc Thư Mục

### ESP32-A (Sensor Node)
```
YoloUNO_ESP32A_Sensor/
├── platformio.ini          # Board config, SSID: "ESP32-A SENSOR"
├── include/
│   ├── global.h            # SensorData, Queue, EventGroup, NetConfig_t
│   ├── coreiot.h           # MQTT publish task
│   ├── temp_humi_monitor.h # DHT20 read task
│   ├── led_blinky.h        # LED heartbeat
│   ├── neo_blinky.h        # NeoPixel heartbeat
│   ├── task_check_info.h   # loadNetConfig(), Save/Delete config
│   ├── task_wifi.h         # WiFi STA/AP functions
│   └── task_webserver.h    # Web server declarations
├── src/
│   ├── main.cpp            # setup() + loop(), tạo 4 tasks
│   ├── global.cpp          # Queue + EventGroup instance
│   ├── temp_humi_monitor.cpp  # DHT20 → Queue
│   ├── coreiot.cpp         # Queue → MQTT publish
│   ├── task_wifi.cpp       # WiFi connect + EventGroup
│   ├── task_check_info.cpp # LittleFS config read/write
│   ├── task_webserver.cpp  # /connect + /sensor API
│   ├── led_blinky.cpp      # Heartbeat LED
│   └── neo_blinky.cpp      # Heartbeat NeoPixel
└── data/
    └── index.html          # Web form cấu hình
```

### ESP32-B (Controller Node)
```
YoloUNO_PlatformIO-RTOS_Project/
├── platformio.ini          # Board config, SSID: "ESP32 LOCAL"
├── include/
│   └── (9 header files)    # Thêm fan_monitor.h
├── src/
│   └── (12 source files)   # Thêm fan_monitor.cpp, tinyml.cpp, task_core_iot.cpp
└── data/
    └── index.html          # Dashboard HTML + WebSocket
```

---

## 11. Hướng Dẫn Triển Khai Từng Bước

### Checklist công việc

```
Phase 1: Hạ tầng
  □ Cài Mosquitto MQTT Broker trên máy tính
  □ Test broker hoạt động (mosquitto_pub/sub)
  □ Tạo 2 device trên CoreIOT (app.coreiot.io)
  □ Copy Access Token của mỗi device
  □ Tạo Rule Chain chuyển tiếp telemetry → shared attributes

Phase 2: ESP32-A (Sensor)
  □ Mở project ESP32-A trong VSCode
  □ Sửa FALLBACK_SERVER trong coreiot.cpp → IP máy tính
  □ Sửa FALLBACK_TOKEN → Token device ESP32-A trên CoreIOT
  □ Build (✔️) → Kiểm tra SUCCESS
  □ Upload firmware (→)
  □ Upload filesystem: pio run --target uploadfs
  □ Mở Serial Monitor (115200 baud)
  □ Kết nối WiFi "ESP32-A SENSOR" (pass: 12345678)
  □ Truy cập 192.168.4.1 → Nhập WiFi nhà + Token + Server
  □ ESP32-A restart → Kiểm tra Serial: MQTT connected + Publishing

Phase 3: ESP32-B (Controller)
  □ Mở project ESP32-B trong VSCode
  □ Sửa coreIOT_Server trong coreiot.cpp → IP máy tính
  □ Sửa coreIOT_Token → Token device ESP32-B trên CoreIOT
  □ Build + Upload firmware + Upload filesystem
  □ Cấu hình WiFi qua AP "ESP32 LOCAL" → 192.168.4.1
  □ Kiểm tra Serial: MQTT connected + Receiving shared attributes
  □ Kiểm tra LCD hiện nhiệt độ/độ ẩm
  □ Kiểm tra LED và NeoPixel phản ứng theo ngưỡng

Phase 4: Tích hợp
  □ Cả 2 ESP32 cùng kết nối WiFi nhà
  □ ESP32-A publish → CoreIOT → Rule Chain → ESP32-B nhận
  □ Kiểm tra Web Dashboard ESP32-B hiện data real-time
  □ Test điều khiển quạt từ Web (ON/OFF/AUTO)
  □ Test các ngưỡng cảnh báo (thổi hơi nóng vào DHT20 hoặc dùng mosquitto_pub)
```

---

## 12. Troubleshooting

| Triệu chứng | Nguyên nhân | Cách xử lý |
|---|---|---|
| Serial: `[MQTT] failed (rc=-2)` | Không tìm được broker | Kiểm tra IP broker, firewall Windows, Mosquitto có chạy không |
| Serial: `[MQTT] failed (rc=5)` | Token sai hoặc thiếu | Kiểm tra Access Token trên CoreIOT |
| LCD: `No Signal...` | ESP32-B không nhận data | Kiểm tra Rule Chain CoreIOT, ESP32-A có publish không |
| LED/NeoPixel: chỉ heartbeat | Chưa nhận data lần nào | Kiểm tra MQTT subscribe topic, thử `mosquitto_pub` manual |
| Web 192.168.4.1 không mở | Chưa kết nối WiFi AP | Kết nối WiFi `ESP32-A SENSOR` hoặc `ESP32 LOCAL` |
| Build fail: `lcd.init()` | Thư viện LCD dùng `begin()` | Đổi thành `lcd.begin()` |
| Quạt không phản ứng | Manual override đang bật | Gọi `/action?dev=fan&state=AUTO` để trả về chế độ tự động |

### Test thủ công bằng MQTT command line

```bash
# Giả lập ESP32-A gửi data (test ESP32-B nhận)
mosquitto_pub -h <broker_ip> -t "v1/devices/me/telemetry" \
  -u "<token_ESP32A>" \
  -m '{"temperature":35,"humidity":70}'

# Giả lập CoreIOT gửi Shared Attribute (test trực tiếp ESP32-B)
mosquitto_pub -h <broker_ip> -t "v1/devices/me/attributes" \
  -u "<token_ESP32B>" \
  -m '{"remote_temp":40,"remote_hum":85}'
```
