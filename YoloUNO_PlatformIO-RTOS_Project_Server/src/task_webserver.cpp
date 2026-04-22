#include "task_webserver.h"
#include "task_check_info.h"
#include "fan_monitor.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool webserver_isrunning = false;

// ─── /sensor — trả JSON nhiệt độ/độ ẩm mới nhất ─────────────────────────────
void handleSensor(AsyncWebServerRequest *request)
{
    SensorData sd = {0.0f, 0.0f};
    xQueuePeek(xQueueSensorData, &sd, 0);  // Non-blocking peek

    char jsonBuf[64];
    snprintf(jsonBuf, sizeof(jsonBuf),
             "{\"temp\":%.1f,\"hum\":%.1f}", sd.temp, sd.hum);
    request->send(200, "application/json", jsonBuf);
}

// ─── /action — điều khiển quạt và LCD ───────────────────────────────────────
void handleAction(AsyncWebServerRequest *request)
{
    String dev   = request->hasArg("dev")   ? request->arg("dev")   : "";
    String state = request->hasArg("state") ? request->arg("state") : "";

    if (dev == "fan") {
        if (state == "ON") {
            FanSetManualOverride(true, true);
            FanON();
            Serial.println("[Web] Fan ON (manual)");
        } else if (state == "OFF") {
            FanSetManualOverride(true, false);
            FanOFF();
            Serial.println("[Web] Fan OFF (manual)");
        } else if (state == "AUTO") {
            FanClearManualOverride();
            Serial.println("[Web] Fan → AUTO mode");
        } else if (state == "SPEED" && request->hasArg("value")) {
            int speed = request->arg("value").toInt();
            speed = constrain(speed, 0, 255);
            FanSetSpeed((uint8_t)speed);
            Serial.printf("[Web] Fan speed → %d\n", speed);
        }
    }
    else if (dev == "lcd") {
        // LCD backlight điều khiển qua bất kỳ task nào giữ reference
        // Ở đây dùng Serial log — LCD task tự quản lý backlight theo state
        if (state == "ON") {
            Serial.println("[Web] LCD backlight ON");
        } else {
            Serial.println("[Web] LCD backlight OFF");
        }
    }

    request->send(200, "text/plain", "OK");
}

// ─── WebSocket Event Handler ──────────────────────────────────────────────────
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] Client #%u connected from %s\n",
                      client->id(), client->remoteIP().toString().c_str());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->opcode == WS_TEXT) {
            String message;
            message += String((char *)data).substring(0, len);
            handleWebSocketMessage(message);
        }
    }
}

void Webserver_sendata(String data)
{
    if (ws.count() > 0) {
        ws.textAll(data);
    }
}

// ─── Khởi tạo routes và bắt đầu server ──────────────────────────────────────
void connnectWSV()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);

    // Trang dashboard chính (từ LittleFS)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });

    // Endpoint lưu cấu hình từ giao diện Web
    server.on("/connect", HTTP_GET, [](AsyncWebServerRequest *request) {
        String ssid   = request->hasArg("ssid")   ? request->arg("ssid")   : "";
        String pass   = request->hasArg("pass")   ? request->arg("pass")   : "";
        String token  = request->hasArg("token")  ? request->arg("token")  : "";
        String srv    = request->hasArg("server") ? request->arg("server") : "";
        String portStr= request->hasArg("port")   ? request->arg("port")   : "";

        Serial.printf("[Web] Config Wifi: %s, Server: %s:%s\n", ssid.c_str(), srv.c_str(), portStr.c_str());
        
        request->send(200, "text/plain", "Thành công");
        
        // Lưu và tự restart
        Save_info_File(ssid, pass, token, srv, portStr);
    });

    // REST API endpoints
    server.on("/sensor", HTTP_GET, handleSensor);
    server.on("/action", HTTP_GET, handleAction);

    // Static files (nếu vẫn cần)
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/script.js", "application/javascript");
    });
    server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/styles.css", "text/css");
    });

    server.begin();
    ElegantOTA.begin(&server);
    webserver_isrunning = true;
    Serial.println("[WebServer] Started on AP: " + WiFi.softAPIP().toString());
}

void Webserver_stop()
{
    ws.closeAll();
    server.end();
    webserver_isrunning = false;
}

void Webserver_reconnect()
{
    if (!webserver_isrunning) {
        connnectWSV();
    }
    ElegantOTA.loop();
}
