#include "task_wifi.h"
#include "task_check_info.h"

void startAP()
{
    WiFi.mode(WIFI_AP);
    // Lưu ý: SSID_AP và PASS_AP lấy từ platformio.ini (build macros)
    WiFi.softAP(String(SSID_AP), String(PASS_AP));
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}

void startSTA()
{
    NetConfig_t cfg;
    loadNetConfig(&cfg);

    if (strlen(cfg.ssid) == 0)
    {
        vTaskDelete(NULL);
    }

    WiFi.mode(WIFI_STA);

    if (strlen(cfg.pass) == 0)
    {
        WiFi.begin(cfg.ssid);
    }
    else
    {
        WiFi.begin(cfg.ssid, cfg.pass);
    }

    while (WiFi.status() != WL_CONNECTED)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    // Set the Wifi Event Group Connected Bit (FreeRTOS standard)
    if (egWifiStatus != NULL) {
        xEventGroupSetBits(egWifiStatus, WIFI_CONNECTED_BIT);
    }
}

bool Wifi_reconnect()
{
    const wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED)
    {
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
