#include "task_check_info.h"

bool loadNetConfig(NetConfig_t* cfg)
{
  memset(cfg, 0, sizeof(NetConfig_t));
  File file = LittleFS.open("/info.dat", "r");
  if (!file) return false;

  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    file.close();
    return false;
  }

  strlcpy(cfg->ssid,       doc["WIFI_SSID"]       | "", sizeof(cfg->ssid));
  strlcpy(cfg->pass,       doc["WIFI_PASS"]       | "", sizeof(cfg->pass));
  strlcpy(cfg->coreToken,  doc["CORE_IOT_TOKEN"]  | "", sizeof(cfg->coreToken));
  strlcpy(cfg->coreServer, doc["CORE_IOT_SERVER"] | "", sizeof(cfg->coreServer));

  if (doc.containsKey("CORE_IOT_PORT")) {
    String portStr = doc["CORE_IOT_PORT"].as<String>();
    cfg->corePort = portStr.toInt();
  }
  if (cfg->corePort == 0) cfg->corePort = 1883;

  file.close();
  return true;
}

void Delete_info_File()
{
  if (LittleFS.exists("/info.dat")) LittleFS.remove("/info.dat");
  ESP.restart();
}

void Save_info_File(String wifi_ssid, String wifi_pass, String token, String server, String port)
{
  Serial.println("[Config] Saving: " + wifi_ssid);

  DynamicJsonDocument doc(4096);
  doc["WIFI_SSID"]       = wifi_ssid;
  doc["WIFI_PASS"]       = wifi_pass;
  doc["CORE_IOT_TOKEN"]  = token;
  doc["CORE_IOT_SERVER"] = server;
  doc["CORE_IOT_PORT"]   = port;

  File configFile = LittleFS.open("/info.dat", "w");
  if (configFile) {
    serializeJson(doc, configFile);
    configFile.close();
  } else {
    Serial.println("Unable to save configuration.");
  }
  ESP.restart();
}

bool check_info_File(bool isLoopMode)
{
  if (!isLoopMode) {
    if (!LittleFS.begin(true)) {
      Serial.println(F("❌ LittleFS mount failed!"));
      return false;
    }
  }

  NetConfig_t cfg;
  if (!loadNetConfig(&cfg)) {
    if (!isLoopMode) startAP();
    return false;
  }

  if (strlen(cfg.ssid) == 0 && strlen(cfg.pass) == 0) {
    if (!isLoopMode) startAP();
    return false;
  }
  return true;
}