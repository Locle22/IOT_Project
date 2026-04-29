#include "task_wifi.h"
#include "task_check_info.h"

// Flag: đang ở AP mode chờ user nhập WiFi mới → KHÔNG thử reconnect
static bool apFallbackActive = false;

void startAP()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(String(SSID_AP), String(PASS_AP));
    Serial.print("[WiFi] AP Started — IP: ");
    Serial.println(WiFi.softAPIP());
    apFallbackActive = true;   // Khóa reconnect
}

void startSTA()
{
    NetConfig_t cfg;
    loadNetConfig(&cfg);

    if (strlen(cfg.ssid) == 0) {
        Serial.println("[WiFi] No SSID configured, starting AP...");
        startAP();
        return;
    }

    apFallbackActive = false;  // Mở khóa vì có config mới
    WiFi.mode(WIFI_STA);

    if (strlen(cfg.pass) == 0) {
        WiFi.begin(cfg.ssid);
    } else {
        WiFi.begin(cfg.ssid, cfg.pass);
    }

    Serial.printf("[WiFi] Connecting to %s", cfg.ssid);

    // Timeout 15 giây — nếu thất bại thì quay lại AP
    int timeout = 150;  // 150 x 100ms = 15 giây
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        Serial.print(".");
        timeout--;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" Connected!");
        Serial.print("[WiFi] IP: ");
        Serial.println(WiFi.localIP());
        if (egWifiStatus != NULL) {
            xEventGroupSetBits(egWifiStatus, WIFI_CONNECTED_BIT);
        }
    } else {
        Serial.println("\n[WiFi] ❌ Connection failed! Staying in AP mode...");
        WiFi.disconnect();
        startAP();
    }
}

bool Wifi_reconnect()
{
    // Nếu đang ở AP fallback → không thử reconnect, chờ user nhập WiFi mới
    if (apFallbackActive) {
        return false;
    }

    const wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
        if (egWifiStatus != NULL) {
            xEventGroupSetBits(egWifiStatus, WIFI_CONNECTED_BIT);
        }
        return true;
    }
    if (egWifiStatus != NULL) {
        xEventGroupClearBits(egWifiStatus, WIFI_CONNECTED_BIT);
    }
    startSTA();
    return false;
}
