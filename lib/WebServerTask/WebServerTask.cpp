#include "WebServerTask.h"

WebServer server(80);

// Biến lưu thông tin WiFi
const char* ssid = "";
const char* password = "";

// Biến lưu nhiệt độ và độ ẩm để hiển thị trên web
static double webTemp = 0;
static double webHum = 0;

// Trạng thái quạt và LCD backlight
static bool fanState = false;
static bool lcdBacklightState = true;

// HTML page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Bảng Điều Khiển Thiết Bị</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #e0f7fa 0%, #b2ebf2 50%, #e1f5fe 100%); margin: 0; padding: 20px; text-align: center; min-height: 100vh; }
    h2 { color: #1c1e21; margin-bottom: 30px; }
    .main-container { display: flex; flex-wrap: wrap; justify-content: center; align-items: flex-start; gap: 20px; max-width: 900px; margin: 0 auto; }
    .left-column { display: flex; flex-direction: column; gap: 15px; }
    .sensor-card { border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); padding: 25px; width: 350px; box-sizing: border-box; display: flex; flex-direction: column; justify-content: center; color: white; transition: background-color 0.5s ease; background-color: #00bcd4; }
    .sensor-title { font-size: 18px; font-weight: bold; margin-bottom: 20px; text-shadow: 1px 1px 2px rgba(0,0,0,0.3); }
    .sensor-row { display: flex; align-items: center; justify-content: center; margin: 15px 0; }
    .sensor-icon { font-size: 50px; margin-right: 15px; }
    .sensor-value { font-size: 42px; font-weight: bold; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }
    .sensor-unit { font-size: 24px; margin-left: 5px; }
    .clock-card { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); padding: 20px; width: 350px; box-sizing: border-box; color: white; }
    .clock-title { font-size: 16px; font-weight: bold; margin-bottom: 10px; text-shadow: 1px 1px 2px rgba(0,0,0,0.3); }
    .clock-time { font-size: 36px; font-weight: bold; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }
    .clock-date { font-size: 16px; margin-top: 8px; opacity: 0.9; }
    .controls-column { display: flex; flex-direction: column; gap: 15px; }
    .card { background-color: white; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); padding: 25px; width: 350px; box-sizing: border-box; }
    .device-name { font-size: 20px; font-weight: bold; color: #333; margin-bottom: 20px; }
    .btn { border: none; border-radius: 8px; padding: 15px 35px; font-size: 18px; font-weight: bold; cursor: pointer; transition: 0.2s; margin: 0 10px; }
    .btn-on { background-color: #e8ebe4; color: #4b4f56; }
    .btn-off { background-color: #e8ebe4; color: #4b4f56; }
    .btn-on.active { background-color: #11b911; color: white; }
    .btn-off.active { background-color: #dc3545; color: white; }
    .btn:active { transform: scale(0.95); }
    .alert-box { margin-top: 12px; padding: 10px 15px; border-radius: 8px; font-size: 14px; font-weight: 600; display: flex; align-items: center; gap: 8px; }
    .alert-warning { background-color: #fff3cd; color: #856404; border: 1px solid #ffc107; }
    .alert-ok { background-color: #d4edda; color: #155724; border: 1px solid #28a745; }
    .slider-wrap { margin-top: 14px; text-align: left; }
    .slider-label { display: flex; justify-content: space-between; font-size: 14px; color: #444; margin-bottom: 8px; }
    .slider-input { width: 100%; accent-color: #0d9f3d; }
  </style>
</head>
<body>
  <h2>ESP32 DASHBOARD </h2>
  <div class="main-container">
    <div class="left-column">
      <div class="sensor-card" id="sensorCard">
        <div class="sensor-title">📊 THÔNG SỐ MÔI TRƯỜNG</div>
        <div class="sensor-row">
          <span class="sensor-icon">🌡️</span>
          <span class="sensor-value" id="tempValue">--</span>
          <span class="sensor-unit">°C</span>
        </div>
        <div class="sensor-row">
          <span class="sensor-icon">💧</span>
          <span class="sensor-value" id="humidValue">--</span>
          <span class="sensor-unit">%</span>
        </div>
      </div>
      <div class="clock-card">
        <div class="clock-title">🕐 THỜI GIAN HIỆN TẠI</div>
        <div class="clock-time" id="clockTime">00:00:00</div>
        <div class="clock-date" id="clockDate">--/--/----</div>
      </div>
    </div>
    <div class="controls-column">
      <div class="card">
        <div class="device-name">❄️ Quạt Làm Mát</div>
        <button class="btn btn-on" id="fanOnBtn" onclick="controlDevice('fan', 'ON')">BẬT</button>
        <button class="btn btn-off active" id="fanOffBtn" onclick="controlDevice('fan', 'OFF')">TẮT</button>
        <div class="slider-wrap">
          <div class="slider-label">
            <span>Tốc độ quạt</span>
            <span id="fanSpeedValue">255</span>
          </div>
          <input class="slider-input" type="range" id="fanSpeedSlider" min="0" max="255" value="255" oninput="onFanSpeedInput(this.value)" onchange="setFanSpeed(this.value)">
        </div>
        <div class="alert-box alert-warning" id="fanAlert">
          <span>⚠️</span>
          <span>Quạt đang tắt</span>
        </div>
      </div>
      <div class="card">
        <div class="device-name">💡 Đèn LCD</div>
        <button class="btn btn-on active" id="lcdOnBtn" onclick="controlDevice('lcd', 'ON')">BẬT</button>
        <button class="btn btn-off" id="lcdOffBtn" onclick="controlDevice('lcd', 'OFF')">TẮT</button>
        <div class="alert-box alert-ok" id="lcdAlert">
          <span>✅</span>
          <span>LCD đang bật</span>
        </div>
      </div>
    </div>
  </div>
  <script>
    var temperature = 0;
    var humidity = 0;
    var fanState = false;
    var lcdState = true;
    var fanSpeed = 255;

    function controlDevice(device, state) {
      fetch('/action?dev=' + device + '&state=' + state)
        .then(function(response) {
          if(!response.ok) { alert("Lỗi kết nối!"); return; }
          return response.text();
        })
        .then(function(data) {
          if(device === 'fan') {
            fanState = (state === 'ON');
            updateFanButtons();
            updateFanAlert();
            if (fanState) {
              setFanSpeed(fanSpeed);
            }
          } else if(device === 'lcd') {
            lcdState = (state === 'ON');
            updateLcdButtons();
            updateLcdAlert();
          }
        })
        .catch(function(err) { console.log('Error:', err); });
    }

    function updateFanButtons() {
      var onBtn = document.getElementById('fanOnBtn');
      var offBtn = document.getElementById('fanOffBtn');
      if(fanState) {
        onBtn.classList.add('active');
        offBtn.classList.remove('active');
      } else {
        onBtn.classList.remove('active');
        offBtn.classList.add('active');
      }
    }

    function updateLcdButtons() {
      var onBtn = document.getElementById('lcdOnBtn');
      var offBtn = document.getElementById('lcdOffBtn');
      if(lcdState) {
        onBtn.classList.add('active');
        offBtn.classList.remove('active');
      } else {
        onBtn.classList.remove('active');
        offBtn.classList.add('active');
      }
    }

    function updateFanAlert() {
      var alertBox = document.getElementById('fanAlert');
      if(fanState) {
        alertBox.className = 'alert-box alert-ok';
        alertBox.innerHTML = '<span>✅</span><span>Quạt đang bật</span>';
      } else {
        alertBox.className = 'alert-box alert-warning';
        alertBox.innerHTML = '<span>⚠️</span><span>Quạt đang tắt</span>';
      }
    }

    function updateLcdAlert() {
      var alertBox = document.getElementById('lcdAlert');
      if(lcdState) {
        alertBox.className = 'alert-box alert-ok';
        alertBox.innerHTML = '<span>✅</span><span>LCD đang bật</span>';
      } else {
        alertBox.className = 'alert-box alert-warning';
        alertBox.innerHTML = '<span>⚠️</span><span>LCD đang tắt</span>';
      }
    }

    function onFanSpeedInput(value) {
      fanSpeed = Number(value);
      document.getElementById('fanSpeedValue').textContent = fanSpeed;
    }

    function setFanSpeed(value) {
      fanSpeed = Number(value);
      document.getElementById('fanSpeedValue').textContent = fanSpeed;
      fetch('/action?dev=fan&state=SPEED&value=' + fanSpeed)
        .then(function(response) {
          if(!response.ok) { console.log('Set speed failed'); }
        })
        .catch(function(err) { console.log('Error:', err); });
    }

    function updateSensorCardColor(temp) {
      var minTemp = 15, maxTemp = 45;
      temp = Math.max(minTemp, Math.min(maxTemp, temp));
      var ratio = (temp - minTemp) / (maxTemp - minTemp);
      var r, g, b;
      if (ratio < 0.5) {
        var t = ratio * 2;
        r = Math.round(0 + t * 255);
        g = Math.round(188 + t * (193 - 188));
        b = Math.round(212 - t * (212 - 7));
      } else {
        var t = (ratio - 0.5) * 2;
        r = Math.round(255 - t * (255 - 244));
        g = Math.round(193 - t * (193 - 67));
        b = Math.round(7 + t * (54 - 7));
      }
      document.getElementById('sensorCard').style.backgroundColor = 'rgb(' + r + ',' + g + ',' + b + ')';
    }

    function fetchSensorData() {
      fetch('/sensor')
        .then(function(response) { return response.json(); })
        .then(function(data) {
          temperature = data.temp;
          humidity = data.hum;
          document.getElementById('tempValue').textContent = temperature.toFixed(1);
          document.getElementById('humidValue').textContent = humidity.toFixed(1);
          updateSensorCardColor(temperature);
        });
    }

    function updateClock() {
      var now = new Date();
      var hours = String(now.getHours()).padStart(2, '0');
      var minutes = String(now.getMinutes()).padStart(2, '0');
      var seconds = String(now.getSeconds()).padStart(2, '0');
      var day = String(now.getDate()).padStart(2, '0');
      var month = String(now.getMonth() + 1).padStart(2, '0');
      var year = now.getFullYear();
      var weekdays = ['Chủ Nhật', 'Thứ Hai', 'Thứ Ba', 'Thứ Tư', 'Thứ Năm', 'Thứ Sáu', 'Thứ Bảy'];
      var weekday = weekdays[now.getDay()];
      document.getElementById('clockTime').textContent = hours + ':' + minutes + ':' + seconds;
      document.getElementById('clockDate').textContent = weekday + ', ' + day + '/' + month + '/' + year;
    }

    updateClock();
    setInterval(updateClock, 1000);
    fetchSensorData();
    setInterval(fetchSensorData, 2000);
  </script>
</body>
</html>
)rawliteral";

// Hàm điều khiển LCD backlight
void LCDBacklightON() {
    lcdBacklightState = true;
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
        lcd.backlight();
        xSemaphoreGive(i2cMutex);
    }
    Serial.println("LCD Backlight ON");
}

void LCDBacklightOFF() {
    lcdBacklightState = false;
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
        lcd.noBacklight();
        xSemaphoreGive(i2cMutex);
    }
    Serial.println("LCD Backlight OFF");
}

// Handler cho trang chính
void handleRoot() {
    server.send(200, "text/html", index_html);
}

// Handler cho dữ liệu sensor (JSON)
void handleSensor() {
    // Đọc dữ liệu từ sensor
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        dht20.read();
        webTemp = dht20.getTemperature();
        webHum = dht20.getHumidity();
        xSemaphoreGive(i2cMutex);
    }
    
    String json = "{\"temp\":" + String(webTemp, 1) + ",\"hum\":" + String(webHum, 1) + "}";
    server.send(200, "application/json", json);
}

// Handler cho điều khiển thiết bị
void handleAction() {
    String device = server.arg("dev");
    String state = server.arg("state");
    
    if (device == "fan") {
        if (state == "ON") {
      FanSetManualOverride(true, true);
            FanON();
      fanState = true;
      Serial.println("Fan manual override: ON");
    } else if (state == "SPEED") {
      int speed = server.arg("value").toInt();
      speed = constrain(speed, 0, 255);
      FanSetSpeed((uint8_t)speed);
      Serial.print("Fan speed set to: ");
      Serial.println(speed);
    } else if (state == "AUTO") {
      FanClearManualOverride();
      fanState = FanGetState();
      Serial.println("Fan manual override: AUTO");
        } else {
      FanSetManualOverride(true, false);
            FanOFF();
      fanState = false;
      Serial.println("Fan manual override: OFF");
        }
    } 
    else if (device == "lcd") {
        if (state == "ON") {
            LCDBacklightON();
        } else {
            LCDBacklightOFF();
        }
    }
    
    server.send(200, "text/plain", "OK");
}

// Khởi tạo WiFi AP và Web Server
void initWebServer(const char* wifiSSID, const char* wifiPassword) {
    ssid = wifiSSID;
    password = wifiPassword;
    
  // ESP32 tự phát WiFi để thiết bị khác kết nối trực tiếp
  WiFi.mode(WIFI_AP);
  Serial.print("Starting AP: ");
  Serial.println(ssid);

  bool apStarted = WiFi.softAP(ssid, password);
  if (apStarted) {
    Serial.println("AP started!");
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("AP start failed!");
  }
    
    // Cấu hình các route
    server.on("/", handleRoot);
    server.on("/sensor", handleSensor);
    server.on("/action", handleAction);
    
    // Khởi động server
    server.begin();
    Serial.println("Web Server started!");
}

// Task xử lý Web Server
void TaskWebServer(void *pvParameters) {
    while(1) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
