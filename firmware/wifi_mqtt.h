#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <Arduino.h>
#include <DNSServer.h>
#include <PubSubClient.h>
#include "config.h"

#if defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  typedef WebServer WebServerClass;
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  typedef ESP8266WebServer WebServerClass;
#endif

// Networking Globals
extern WiFiClient wifiClient;
extern PubSubClient mqttClient;
extern DNSServer dnsServer;
extern WebServerClass* server;

// Network State machine
enum NetworkState {
  STATE_AP_MODE,
  STATE_CONNECTING,
  STATE_CONNECTED,
  STATE_ESPNOW_ONLY
};
extern NetworkState currentNetState;

// Configuration variables
extern unsigned long lastWifiRetryTime;
extern uint8_t wifiRetryCount;
const uint8_t MAX_WIFI_RETRIES = 3;
const unsigned long WIFI_RETRY_INTERVAL_MS = 10000; // 10 seconds

// Custom captive portal HTML
const char CONFIG_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>IoT OS Setup</title>
  <style>
    body { font-family: -apple-system, sans-serif; background: #0f172a; color: #f8fafc; padding: 20px; display: flex; justify-content: center; }
    .card { background: #1e293b; padding: 30px; border-radius: 12px; max-width: 400px; width: 100%; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.1); }
    h2 { margin-top: 0; color: #38bdf8; font-weight: 600; text-align: center; }
    .field { margin-bottom: 16px; }
    label { display: block; margin-bottom: 6px; font-size: 14px; color: #94a3b8; }
    input[type="text"], input[type="password"], input[type="number"] { width: 100%; padding: 10px; border: 1px solid #334155; border-radius: 6px; background: #0f172a; color: #fff; box-sizing: border-box; }
    .checkbox-field { display: flex; align-items: center; gap: 8px; margin-top: 12px; }
    input[type="checkbox"] { width: 18px; height: 18px; accent-color: #38bdf8; }
    button { width: 100%; padding: 12px; background: #0284c7; border: none; border-radius: 6px; color: #fff; font-size: 16px; font-weight: bold; cursor: pointer; margin-top: 15px; }
    button:hover { background: #0369a1; }
    .info { font-size: 12px; color: #64748b; text-align: center; margin-top: 20px; }
  </style>
</head>
<body>
  <div class="card">
    <h2>IoT OS Configuration</h2>
    <form action="/save" method="POST">
      <div class="field">
        <label>WiFi SSID</label>
        <input type="text" name="ssid" required placeholder="Select Network SSID">
      </div>
      <div class="field">
        <label>WiFi Password</label>
        <input type="password" name="pass" placeholder="Network Password">
      </div>
      <div class="field">
        <label>MQTT Broker Address</label>
        <input type="text" name="mqtt_host" required placeholder="e.g. 192.168.1.100">
      </div>
      <div class="field">
        <label>MQTT Port</label>
        <input type="number" name="mqtt_port" value="1883">
      </div>
      #if_esp32_field#
      <button type="submit">Save and Reboot</button>
    </form>
    <div class="info">Cleaned MAC: #cleaned_mac#</div>
  </div>
</body>
</html>
)rawhtml";

// Captive portal handlers
inline void handleRoot() {
  String html = FPSTR(CONFIG_HTML);
  html.replace("#cleaned_mac#", cleanedMac);
  
  #if defined(ESP32)
    // Only show Gateway option on ESP32
    html.replace("#if_esp32_field#", 
      "<div class='field checkbox-field'>"
      "<input type='checkbox' id='is_gw' name='is_gw' value='true'>"
      "<label for='is_gw'>Act as central ESP-NOW Gateway</label>"
      "</div>"
    );
  #else
    html.replace("#if_esp32_field#", "");
  #endif

  server->send(200, "text/html", html);
}

inline void handleSave() {
  if (!server->hasArg("ssid") || !server->hasArg("mqtt_host")) {
    server->send(400, "text/plain", "SSID and MQTT Host are required.");
    return;
  }

  strlcpy(globalConfig.wifi_ssid, server->arg("ssid").c_str(), sizeof(globalConfig.wifi_ssid));
  strlcpy(globalConfig.wifi_pass, server->arg("pass").c_str(), sizeof(globalConfig.wifi_pass));
  strlcpy(globalConfig.mqtt_server, server->arg("mqtt_host").c_str(), sizeof(globalConfig.mqtt_server));
  globalConfig.mqtt_port = server->arg("mqtt_port").toInt();
  
  #if defined(ESP32)
    globalConfig.is_gateway = server->hasArg("is_gw") && server->arg("is_gw") == "true";
  #else
    globalConfig.is_gateway = false;
  #endif
  
  globalConfig.config_version = 1;

  server->send(200, "text/html", "<h3>Configuration Saved. Rebooting device...</h3>");
  delay(1000);
  
  saveConfig();
  ESP.restart();
}

// Start local Captive Portal AP Mode
inline void startAPMode() {
  currentNetState = STATE_AP_MODE;
  WiFi.mode(WIFI_AP);
  
  String apSSID = "ESP_Kurulum_AP_" + cleanedMac.substring(8);
  WiFi.softAP(apSSID.c_str());
  
  // Set up captive portal DNS
  dnsServer.start(53, "*", WiFi.softAPIP());
  
  server = new WebServerClass(80);
  server->on("/", HTTP_GET, handleRoot);
  server->on("/save", HTTP_POST, handleSave);
  server->onNotFound(handleRoot); // Redirect all other requests to config page (Captive Portal)
  server->begin();
  
  Serial.print(F("[WiFi] Captive Portal active. SSID: "));
  Serial.println(apSSID);
  Serial.print(F("[WiFi] Config Portal IP: "));
  Serial.println(WiFi.softAPIP());
}

// MQTT Message Callback handler
inline void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Avoid heap fragmentation: reserve string buffer
  String payloadStr;
  payloadStr.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }

  String topicStr = String(topic);

  // 1. Dynamic Config (Pins / Rules) Update
  if (topicStr.startsWith(F("cihaz/config/"))) {
    Serial.println(F("[MQTT] Received new configuration payload..."));
    
    StaticJsonDocument<1536> doc;
    DeserializationError err = deserializeJson(doc, payloadStr);
    if (!err && doc.is<JsonArray>()) {
      JsonArray arr = doc.as<JsonArray>();
      if (arr.size() > 0) {
        JsonObject first = arr[0].as<JsonObject>();
        if (first.containsKey("m")) {
          // Mode key exists -> It is a Pin Configuration Array
          Serial.println(F("[MQTT] Saving pins.json..."));
          File pinFile = FS_SYSTEM.open(F("/pins.json"), "w");
          if (pinFile) {
            pinFile.print(payloadStr);
            pinFile.close();
            extern void reloadPeripheralsDynamic();
            reloadPeripheralsDynamic();
          }
        } 
        else if (first.containsKey("s")) {
          // SourcePin key exists -> It is a Local Rules Configuration Array
          Serial.println(F("[MQTT] Saving rules.json..."));
          File rulesFile = FS_SYSTEM.open(F("/rules.json"), "w");
          if (rulesFile) {
            rulesFile.print(payloadStr);
            rulesFile.close();
            extern void reloadRulesDynamic();
            reloadRulesDynamic();
          }
        }
      }
    }
  }
  // 2. Direct Pin Control Action
  else if (topicStr.startsWith(F("cihaz/kontrol/"))) {
    int lastSlash = topicStr.lastIndexOf('/');
    if (lastSlash != -1) {
      String pinStr = topicStr.substring(lastSlash + 1);
      uint8_t pinNum = pinStr.toInt();
      
      // Route action to PinManager (defined in peripherals.h)
      extern void handlePeripheralAction(uint8_t pin, const char* value);
      handlePeripheralAction(pinNum, payloadStr.c_str());
    }
  }
}

// Trigger non-blocking connecting sequence
inline void connectWiFi() {
  currentNetState = STATE_CONNECTING;
  wifiRetryCount = 0;
  lastWifiRetryTime = millis();
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(globalConfig.wifi_ssid, globalConfig.wifi_pass);
  Serial.printf("[WiFi] Connecting to %s...\n", globalConfig.wifi_ssid);
  
  mqttClient.setCallback(mqttCallback);
}

// Handle WiFi / MQTT Reconnections in loop
inline void checkNetworkLoop() {
  if (currentNetState == STATE_AP_MODE) {
    dnsServer.processNextRequest();
    server->handleClient();
    return;
  }
  
  if (currentNetState == STATE_ESPNOW_ONLY) {
    return; // Working strictly in ESP-NOW mode
  }

  unsigned long currentMillis = millis();

  // If WiFi is not connected
  if (WiFi.status() != WL_CONNECTED) {
    if (currentNetState == STATE_CONNECTED) {
      Serial.println(F("[WiFi] Connection lost. Reconnecting..."));
      currentNetState = STATE_CONNECTING;
      lastWifiRetryTime = currentMillis;
      wifiRetryCount = 0;
      WiFi.begin(globalConfig.wifi_ssid, globalConfig.wifi_pass);
    }
    
    if (currentMillis - lastWifiRetryTime >= WIFI_RETRY_INTERVAL_MS) {
      lastWifiRetryTime = currentMillis;
      wifiRetryCount++;
      Serial.printf("[WiFi] Retry attempt %d/%d...\n", wifiRetryCount, MAX_WIFI_RETRIES);
      
      if (wifiRetryCount >= MAX_WIFI_RETRIES) {
        Serial.println(F("[WiFi] Retries exceeded. Suspending WiFi Client & Switching to ESP-NOW Mode..."));
        
        // Disable WiFi Client to save memory (especially on ESP8266/ESP-01)
        WiFi.disconnect(true);
        currentNetState = STATE_ESPNOW_ONLY;
        
        // This function will be defined in esp_now_bridge.h
        extern void setupEspNowFailover();
        setupEspNowFailover();
        return;
      }
      
      WiFi.begin(globalConfig.wifi_ssid, globalConfig.wifi_pass);
    }
    return;
  }

  // WiFi is connected
  if (currentNetState == STATE_CONNECTING) {
    currentNetState = STATE_CONNECTED;
    Serial.print(F("[WiFi] Connected! IP: "));
    Serial.println(WiFi.localIP());
    activeChannel = WiFi.channel(); // Capture channel!
    
    if (globalConfig.is_gateway) {
      extern void initEspNow();
      initEspNow();
    }
    mqttClient.setServer(globalConfig.mqtt_server, globalConfig.mqtt_port);
    mqttClient.setCallback(mqttCallback);
  }

  // Handle MQTT Connection
  if (!mqttClient.connected()) {
    static unsigned long lastMqttRetry = 0;
    if (currentMillis - lastMqttRetry > 5000) { // Retry MQTT connection every 5s non-blocking
      lastMqttRetry = currentMillis;
      Serial.print(F("[MQTT] Attempting connection..."));
      
      String clientId = "iot_device_" + cleanedMac;
      String statusTopic = "cihaz/durum/" + cleanedMac;
      
      // Connect with LWT (Last Will and Testament)
      if (mqttClient.connect(clientId.c_str(), statusTopic.c_str(), 1, true, "offline")) {
        Serial.println(F(" Connected."));
        
        // Publish online registration status
        String regTopic = "cihaz/kayit/" + cleanedMac;
        mqttClient.publish(regTopic.c_str(), "online", true);
        
        // Subscribe to configs and direct controls
        String configTopic = "cihaz/config/" + cleanedMac;
        String controlTopic = "cihaz/kontrol/" + cleanedMac + "/+";
        mqttClient.subscribe(configTopic.c_str());
        mqttClient.subscribe(controlTopic.c_str());
      } else {
        Serial.print(F(" Failed, rc="));
        Serial.println(mqttClient.state());
      }
    }
  } else {
    mqttClient.loop();
  }
}

#endif
