#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>

#if defined(ESP32)
  #include <FS.h>
  #include <LittleFS.h>
  #include <WiFi.h>
  #define BOARD_TYPE "ESP32"
  #define FS_SYSTEM LittleFS
#elif defined(ESP8266)
  #include <FS.h>
  #include <LittleFS.h>
  #include <ESP8266WiFi.h>
  #define BOARD_TYPE "ESP8266"
  #define FS_SYSTEM LittleFS
#else
  #error "Unsupported board target! Must compile for ESP32 or ESP8266."
#endif

// Configuration Struct
struct DeviceConfig {
  char wifi_ssid[33] = "";
  char wifi_pass[65] = "";
  char mqtt_server[65] = "";
  int mqtt_port = 1883;
  bool is_gateway = false; // Only valid for ESP32
  uint32_t config_version = 0;
};

// Global variables
extern DeviceConfig globalConfig;
extern String cleanedMac;

// Clean MAC address Helper
inline String getCleanedMac() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  char macStr[13];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", 
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

// Initialize LittleFS
inline bool initFS() {
  #if defined(ESP32)
    if (!LittleFS.begin(true)) { // true will format on fail
      Serial.println(F("[FS] LittleFS Mount Failed on ESP32"));
      return false;
    }
  #elif defined(ESP8266)
    if (!LittleFS.begin()) {
      Serial.println(F("[FS] LittleFS Mount Failed. Formatting..."));
      LittleFS.format();
      if (!LittleFS.begin()) {
        Serial.println(F("[FS] LittleFS Mount Failed after format"));
        return false;
      }
    }
  #endif
  Serial.println(F("[FS] LittleFS Initialized."));
  return true;
}

// Load Config from LittleFS
inline bool loadConfig() {
  if (!FS_SYSTEM.exists(F("/config.json"))) {
    Serial.println(F("[FS] No config.json found. Using defaults."));
    return false;
  }

  File configFile = FS_SYSTEM.open(F("/config.json"), "r");
  if (!configFile) {
    Serial.println(F("[FS] Failed to open config.json for reading"));
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    Serial.print(F("[FS] Failed to parse config JSON: "));
    Serial.println(error.f_str());
    return false;
  }

  strlcpy(globalConfig.wifi_ssid, doc["wifi_ssid"] | "", sizeof(globalConfig.wifi_ssid));
  strlcpy(globalConfig.wifi_pass, doc["wifi_pass"] | "", sizeof(globalConfig.wifi_pass));
  strlcpy(globalConfig.mqtt_server, doc["mqtt_server"] | "", sizeof(globalConfig.mqtt_server));
  globalConfig.mqtt_port = doc["mqtt_port"] | 1883;
  globalConfig.is_gateway = doc["is_gateway"] | false;
  globalConfig.config_version = doc["config_version"] | 0;

  // ESP8266 cannot be gateway
  #if defined(ESP8266)
    globalConfig.is_gateway = false;
  #endif

  Serial.println(F("[FS] Configuration loaded successfully:"));
  Serial.printf("  SSID: %s\n", globalConfig.wifi_ssid);
  Serial.printf("  MQTT Server: %s\n", globalConfig.mqtt_server);
  Serial.printf("  Is Gateway: %s\n", globalConfig.is_gateway ? "Yes" : "No");
  return true;
}

// Save Config to LittleFS
inline bool saveConfig() {
  File configFile = FS_SYSTEM.open(F("/config.json"), "w");
  if (!configFile) {
    Serial.println(F("[FS] Failed to open config.json for writing"));
    return false;
  }

  StaticJsonDocument<512> doc;
  doc["wifi_ssid"] = globalConfig.wifi_ssid;
  doc["wifi_pass"] = globalConfig.wifi_pass;
  doc["mqtt_server"] = globalConfig.mqtt_server;
  doc["mqtt_port"] = globalConfig.mqtt_port;
  doc["is_gateway"] = globalConfig.is_gateway;
  doc["config_version"] = globalConfig.config_version;

  if (serializeJson(doc, configFile) == 0) {
    Serial.println(F("[FS] Failed to write to config.json"));
    configFile.close();
    return false;
  }

  configFile.close();
  Serial.println(F("[FS] Configuration saved successfully."));
  return true;
}

#endif
