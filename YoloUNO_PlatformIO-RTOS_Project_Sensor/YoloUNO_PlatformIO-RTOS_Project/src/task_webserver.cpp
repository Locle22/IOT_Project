#include "task_webserver.h"
#include "task_check_info.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool webserver_isrunning = false;

// ─── /sensor — trả JSON nhiệt độ/độ ẩm từ Queue ─────────────────────────────
void handleSensor(AsyncWebServerRequest *request)
{
    SensorData sd = {0.0f, 0.0f};
    xQueuePeek(xQueueSensorData, &sd, 0);

    char jsonBuf[64];
    snprintf(jsonBuf, sizeof(jsonBuf),
             "{\"temp\":%.1f,\"hum\":%.1f}", sd.temp, sd.hum);
    request->send(200, "application/json", jsonBuf);
}

// ─── Khởi tạo routes ────────────────────────────────────────────────────────
void connnectWSV()
{
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });

    // API cấu hình WiFi + CoreIOT từ giao diện Web
    server.on("/connect", HTTP_GET, [](AsyncWebServerRequest *request) {
        String ssid    = request->hasArg("ssid")   ? request->arg("ssid")   : "";
        String pass    = request->hasArg("pass")   ? request->arg("pass")   : "";
        String token   = request->hasArg("token")  ? request->arg("token")  : "";
        String srv     = request->hasArg("server") ? request->arg("server") : "";
        String portStr = request->hasArg("port")   ? request->arg("port")   : "";

        Serial.printf("[Web] Config: SSID=%s Server=%s:%s\n",
                      ssid.c_str(), srv.c_str(), portStr.c_str());

        request->send(200, "text/plain", "Thành công");
        Save_info_File(ssid, pass, token, srv, portStr);
    });

    // API xem sensor data
    server.on("/sensor", HTTP_GET, handleSensor);

    server.begin();
    webserver_isrunning = true;
    Serial.println("[WebServer] Started on AP: " + WiFi.softAPIP().toString());
}

void Webserver_sendata(String data)
{
    if (ws.count() > 0) ws.textAll(data);
}

void Webserver_stop()
{
    ws.closeAll();
    server.end();
    webserver_isrunning = false;
}

void Webserver_reconnect()
{
    if (!webserver_isrunning) connnectWSV();

}
