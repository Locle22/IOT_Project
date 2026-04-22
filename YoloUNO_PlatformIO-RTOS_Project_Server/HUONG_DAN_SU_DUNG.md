# 📘 HƯỚNG DẪN SỬ DỤNG — ESP32-B (YoloUNO RTOS Project)

> **Board:** YoloUNO (ESP32-S3) · **Framework:** Arduino + FreeRTOS  
> **Vai trò:** Vi điều khiển thứ 2 trong hệ thống dual-ESP32 IoT.  
> Nhận dữ liệu cảm biến từ ESP32-A thông qua CoreIOT (MQTT), xử lý và điều khiển thiết bị tương ứng.

---

## 📋 Mục Lục

1. [Kiến Trúc Hệ Thống](#1-kiến-trúc-hệ-thống)
2. [Quy Trình Khởi Động (Boot Sequence)](#2-quy-trình-khởi-động-boot-sequence)
3. [Chi Tiết Các Task RTOS](#3-chi-tiết-các-task-rtos)
4. [Sơ Đồ Chân Kết Nối Phần Cứng](#4-sơ-đồ-chân-kết-nối-phần-cứng)
5. [Hướng Dẫn Chạy Từng Bước](#5-hướng-dẫn-chạy-từng-bước)
6. [Kiểm Tra & Xử Lý Lỗi](#6-kiểm-tra--xử-lý-lỗi)

---

## 1. Kiến Trúc Hệ Thống

```
┌─────────────┐        MQTT Publish        ┌──────────────────┐
│  ESP32-A    │ ──── (Telemetry Data) ────▶ │  Mosquitto       │
│  (Sensor)   │   temp, hum qua WiFi       │  MQTT Broker     │
│  DHT20      │                            │  (máy tính/VPS)  │
└─────────────┘                            └────────┬─────────┘
                                                    │
                                           Rule Chain / Shared Attr
                                                    │
                                                    ▼
                                           ┌──────────────────┐
                                           │  CoreIOT Server  │
                                           │  (app.coreiot.io)│
                                           └────────┬─────────┘
                                                    │
                                        Shared Attribute Update
                                          {remote_temp, remote_hum}
                                                    │
                                                    ▼
                                           ┌──────────────────┐
                                           │  ESP32-B         │
                                           │  (YoloUNO)       │
                                           │  ← BẠN ĐANG ĐÂY │
                                           │                  │
                                           │  • LCD 1602      │
                                           │  • LED đơn       │
                                           │  • NeoPixel RGB   │
                                           │  • Quạt PWM      │
                                           │  • Web Dashboard  │
                                           └──────────────────┘
```

---

## 2. Quy Trình Khởi Động (Boot Sequence)

Sau khi upload firmware và cấp nguồn, ESP32-B sẽ thực hiện lần lượt:

```
POWER ON
  │
  ├─ 1. Serial.begin(115200)
  ├─ 2. Khởi tạo EventGroup WiFi (egWifiStatus)
  ├─ 3. Khởi tạo LittleFS
  │     └─ Đọc file /info.dat (cấu hình WiFi + CoreIOT)
  │
  ├─ 4. Kiểm tra cấu hình:
  │     ├─ CÓ cấu hình  → Tiếp tục
  │     └─ KHÔNG có      → Bật Access Point "ESP32 LOCAL" (pass: 12345678)
  │                         Khởi động Web Server trên 192.168.4.1
  │                         Chờ user nhập cấu hình qua Dashboard
  │
  ├─ 5. Tạo 5 FreeRTOS Tasks song song:
  │     ├─ Task 1: LED Blink         (Priority 2, Stack 2048)
  │     ├─ Task 2: NeoPixel RGB      (Priority 2, Stack 2048)
  │     ├─ Task 3: LCD Monitor       (Priority 3, Stack 4096)
  │     ├─ Task 4: Fan Control       (Priority 2, Stack 2048)
  │     └─ Task 5: CoreIOT/MQTT      (Priority 2, Stack 8192)
  │
  └─ 6. Loop():
        ├─ Kiểm tra và duy trì kết nối WiFi STA
        └─ Khởi động/duy trì Web Server (AP mode)
```

---

## 3. Chi Tiết Các Task RTOS

### Task 1: LED Blink — Phản ánh trạng thái Nhiệt Độ
| Trạng thái | Điều kiện | Hành vi LED (GPIO 48) |
|:---:|---|---|
| 🟢 NORMAL | temp < 30°C | Nhấp nháy chậm: 1s bật / 1s tắt |
| 🟡 WARNING | 30°C ≤ temp < 38°C | Double-blink: 2 lần (300ms bật / 150ms tắt), nghỉ 700ms |
| 🔴 CRITICAL | temp ≥ 38°C | Blink nhanh SOS: 5 lần (100ms bật / 100ms tắt) |
| ⚪ Chưa có data | — | Heartbeat nhẹ: 200ms bật / 800ms tắt |

### Task 2: NeoPixel RGB — Phản ánh trạng thái Độ Ẩm
| Trạng thái | Điều kiện | Màu NeoPixel (GPIO 45) |
|:---:|---|---|
| 🟢 NORMAL | hum < 60% | Xanh lá sáng đều |
| 🟠 WARNING | 60% ≤ hum < 80% | Cam sáng đều |
| 🔴 CRITICAL | hum ≥ 80% | Đỏ nhấp nháy nhanh (200ms) |
| 🔵 Chưa có data | — | Xanh dương mờ |

### Task 3: LCD 1602 Monitor — Hiển thị dữ liệu cảm biến
| Trạng thái | Nội dung hiển thị |
|:---:|---|
| 🟢 NORMAL | Dòng 1: `Nhiet do:25.3'C` · Dòng 2: `Do am:  55.0 %` |
| 🟡 WARNING | Dòng 1: `CANH BAO! NHIET` · Dòng 2: `T:32.5C H:65%` |
| 🔴 CRITICAL | Dòng 1: `!! NGUY HIEM !!` · Dòng 2: `T:40.2C H:85%!` |
| ❌ Mất tín hiệu | Dòng 1: `No Signal...` · Dòng 2: `Check ESP32-A` |

### Fan Control — Điều khiển quạt PWM
| Chế độ | Nguồn lệnh | Hành vi |
|---|---|---|
| **AUTO** | Semaphore nhiệt độ | Warning/Critical → Bật quạt · Normal → Tắt quạt |
| **Manual ON** | Nút BẬT trên Web | Bật quạt theo tốc độ đã set (0–255) |
| **Manual OFF** | Nút TẮT trên Web | Tắt quạt |

### Task 5: CoreIOT/MQTT — Nhận dữ liệu từ cloud
- Chờ WiFi kết nối thành công (qua EventGroup)
- Kết nối MQTT broker với token xác thực
- Subscribe: `v1/devices/me/attributes` + `v1/devices/me/rpc/request/+`
- Khi nhận `{remote_temp, remote_hum}` → ghi vào Queue + cập nhật Semaphore

---

## 4. Sơ Đồ Chân Kết Nối Phần Cứng

```
YoloUNO (ESP32-S3)
┌──────────────────────────────────┐
│                                  │
│  GPIO 48 ──── LED đơn            │
│  GPIO 45 ──── NeoPixel WS2812B   │
│  GPIO 6  ──── Quạt DC (qua MOSFET)│
│  GPIO 11 ──── I2C SDA (LCD 1602) │
│  GPIO 12 ──── I2C SCL (LCD 1602) │
│                                  │
│  LCD I2C Address: 0x27           │
│  USB-C  ──── Serial Monitor      │
│                                  │
└──────────────────────────────────┘
```

---

## 5. Hướng Dẫn Chạy Từng Bước

### Bước 2: Upload Firmware
1. Mở project trong VSCode
2. Nhấn **✔️ Build** ở thanh dưới → chờ `SUCCESS`
3. Nhấn **→ Upload** ở thanh dưới → chờ firmware nạp xong

### Bước 3: Upload File Hệ Thống (Dashboard HTML)
> ⚠️ **QUAN TRỌNG** — Cần upload nội dung thư mục `data/` vào bộ nhớ LittleFS của ESP32.

1. Copy file `test.html` vào thư mục `data/` và đổi tên thành `index.html`
   ```
   data/
   └── index.html    ← file dashboard của bạn
   ```
2. Trong PlatformIO, mở Terminal và chạy:
   ```bash
   pio run --target uploadfs
   ```
3. Chờ upload hoàn tất

### Bước 4: Cấu Hình WiFi & CoreIOT Lần Đầu
1. Mở **Serial Monitor** (115200 baud) để theo dõi log
2. Vì chưa có cấu hình, ESP32 sẽ tự bật **Access Point**:
   ```
   📶 SSID: ESP32 LOCAL
   🔑 Password: 12345678
   ```
3. Dùng điện thoại/laptop kết nối vào WiFi `ESP32 LOCAL`
4. Mở trình duyệt, truy cập `http://192.168.4.1`
5. Nhấn nút **⚙️ (Cài đặt)** ở góc phải trên
6. Nhập thông tin:
   | Trường | Giá trị mẫu |
   |--------|-------------|
   | Tên Wi-Fi (SSID) | `Tên_WiFi_Nhà_Bạn` |
   | Mật khẩu | `password_wifi` |
   | TOKEN CORE IOT | `g7drm1amhd3dchr379xu` |
   | Máy chủ | `app.coreiot.io` hoặc IP Mosquitto |
   | Cổng | `1883` |
7. Nhấn **🚀 Kết nối** → ESP32 lưu cấu hình và **tự khởi động lại**

### Bước 5: Xác Nhận Hoạt Động
Sau khi restart, kiểm tra Serial Monitor:
```
[CoreIOT] Waiting for WiFi... WiFi ready!
[CoreIOT] Broker: app.coreiot.io:1883
[MQTT] Connecting... connected!
[MQTT] Subscribed: RPC + Attributes
```

### Bước 6: Gửi Dữ Liệu Từ ESP32-A
Đảm bảo ESP32-A (sensor node) đang:
1. Đọc cảm biến DHT20
2. Publish telemetry lên MQTT broker
3. CoreIOT Rule Chain chuyển tiếp thành Shared Attribute `{remote_temp, remote_hum}` tới device ESP32-B

Khi ESP32-B nhận được data, Serial sẽ hiện:
```
[MQTT] Topic: v1/devices/me/attributes | Payload: {"remote_temp":28.5,"remote_hum":65.2}
[Sensor] T=28.5°C H=65.2% | TempSem=NORMAL HumSem=WARNING
```

### Bước 7: Sử Dụng Web Dashboard
1. Kết nối điện thoại/laptop vào WiFi `ESP32 LOCAL` (AP vẫn hoạt động song song)
2. Truy cập `http://192.168.4.1`
3. Dashboard hiển thị:
   - 🌡️ Nhiệt độ và 💧 Độ ẩm real-time (cập nhật mỗi 2 giây)
   - ❄️ **Điều khiển Quạt**: BẬT / TẮT / TỰ ĐỘNG + Thanh trượt tốc độ
   - 💡 **Điều khiển LCD**: BẬT / TẮT backlight
   - 🕐 Đồng hồ thời gian thực

---

## 6. Kiểm Tra & Xử Lý Lỗi

### LCD hiện "No Signal... Check ESP32-A"
- **Nguyên nhân:** Không nhận được data từ MQTT trong 8 giây
- **Xử lý:**
  1. Kiểm tra ESP32-A có đang chạy và gửi data không
  2. Kiểm tra Rule Chain trên CoreIOT đã cấu hình đúng chưa
  3. Kiểm tra Serial Monitor xem MQTT có kết nối thành công không

### LED và NeoPixel chỉ hiện trạng thái "Chưa có data"
- **Nguyên nhân:** Semaphore chưa được set (chưa nhận data lần nào)
- **Xử lý:** Chờ ESP32-A gửi data đầu tiên, hoặc gửi thủ công qua MQTT:
  ```bash
  mosquitto_pub -h <broker_ip> -t "v1/devices/me/attributes" \
    -u "<token>" -m '{"remote_temp":35,"remote_hum":70}'
  ```

### Web Dashboard hiện 0.0 / 0.0
- **Nguyên nhân:** Queue chưa có data
- **Xử lý:** Đợi ESP32-A gửi data hoặc test bằng lệnh mosquitto_pub ở trên

### WiFi không kết nối được
- **Xử lý:**
  1. Giữ nút BOOT trên YoloUNO để xóa cấu hình (nếu đã implement)
  2. Hoặc xóa file cấu hình bằng cách upload lại filesystem: `pio run --target uploadfs`
  3. ESP32 sẽ boot lại chế độ AP để cấu hình lại

---

## 📊 Tóm Tắt Cơ Chế RTOS Sử Dụng

| Cơ chế FreeRTOS | Mục đích | Thay thế cho |
|---|---|---|
| `xQueueSensorData` (Queue size=1) | Truyền data {temp, hum} giữa các Task | Biến toàn cục `float t, h` |
| `semTempNormal/Warning/Critical` | Báo trạng thái nhiệt độ cho LED, LCD, Fan | Biến toàn cục `int tempLevel` |
| `semHumNormal/Warning/Critical` | Báo trạng thái độ ẩm cho NeoPixel | Biến toàn cục `int humLevel` |
| `egWifiStatus` (EventGroup) | Đồng bộ trạng thái WiFi giữa các Task | Biến toàn cục `bool isWifiConnected` |
| `NetConfig_t` (Struct cục bộ) | Đọc cấu hình mạng từ LittleFS khi cần | Biến toàn cục `String WIFI_SSID...` |

> **Không có biến toàn cục nào được sử dụng cho dữ liệu cảm biến, trạng thái mạng, hay cấu hình kết nối.**  
> Toàn bộ giao tiếp giữa các Task đều thông qua Queue, Semaphore và EventGroup — đúng chuẩn FreeRTOS.
