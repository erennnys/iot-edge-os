#ifndef ESP_NOW_BRIDGE_H
#define ESP_NOW_BRIDGE_H

#include <Arduino.h>
#include "config.h"
#include "wifi_mqtt.h"

#if defined(ESP32)
  #include <esp_now.h>
  #include <esp_wifi.h>
  #include <esp_arduino_version.h>
#elif defined(ESP8266)
  #include <espnow.h>
#endif

// Unified ESP-NOW packet structure (248 bytes total, safe under 250 limit)
struct EspNowPacket {
  uint8_t mac[6];
  uint8_t type; // 0 = register (kayit), 1 = report (rapor), 2 = status (durum)
  char payload[240];
};

extern uint8_t activeChannel; // Keep track of last working WiFi channel
extern bool espNowActive;

// Forward declaration of callbacks
#if defined(ESP32)
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    void onDataRecv(const esp_now_recv_info_t * recv_info, const uint8_t *incomingData, int len);
  #else
    void onDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len);
  #endif
  void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
#elif defined(ESP8266)
  void onDataRecv(uint8_t *mac_addr, uint8_t *incomingData, uint8_t len);
  void onDataSent(uint8_t *mac_addr, uint8_t status);
#endif

// Set specific channel on the WiFi interface without connecting
inline void setWiFiChannel(uint8_t channel) {
  if (channel < 1 || channel > 13) channel = 1;
  #if defined(ESP32)
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
  #elif defined(ESP8266)
    wifi_set_channel(channel);
  #endif
  Serial.printf("[ESP-NOW] WiFi interface locked to channel: %d\n", channel);
}

// Initialize ESP-NOW
void initEspNow() {
  if (espNowActive) return;

  #if defined(ESP32)
    if (esp_now_init() != ESP_OK) {
      Serial.println(F("[ESP-NOW] Error initializing ESP-NOW"));
      return;
    }
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
  #elif defined(ESP8266)
    if (esp_now_init() != 0) {
      Serial.println(F("[ESP-NOW] Error initializing ESP-NOW"));
      return;
    }
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
  #endif

  espNowActive = true;
  Serial.println(F("[ESP-NOW] Initialized successfully."));
}

// Send payload via ESP-NOW Broadcast
inline bool sendEspNowBroadcast(uint8_t type, const char* payload) {
  if (!espNowActive) return false;

  EspNowPacket packet;
  // Get self MAC address
  uint8_t selfMac[6];
  WiFi.macAddress(selfMac);

  memcpy(packet.mac, selfMac, 6);
  packet.type = type;
  strlcpy(packet.payload, payload, sizeof(packet.payload));

  uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  #if defined(ESP32)
    // Register broadcast peer dynamically if not present
    if (!esp_now_is_peer_exist(broadcastAddress)) {
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, broadcastAddress, 6);
      peerInfo.channel = activeChannel;
      peerInfo.encrypt = false;
      esp_now_add_peer(&peerInfo);
    }
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&packet, sizeof(packet));
    return result == ESP_OK;
  #elif defined(ESP8266)
    // On ESP8266, add peer first if not registered
    if (!esp_now_is_peer_exist(broadcastAddress)) {
      esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, activeChannel, NULL, 0);
    }
    int result = esp_now_send(broadcastAddress, (uint8_t*)&packet, sizeof(packet));
    return result == 0;
  #endif
}

// Setup ESP-NOW Failover Mode (triggered when WiFi Client fails)
void setupEspNowFailover() {
  Serial.println(F("[Failover] Setting up ESP-NOW only mode..."));
  
  // Set channel to last known working channel of the router
  setWiFiChannel(activeChannel);
  
  initEspNow();
  
  // Broadcast a status packet showing we are offline on WiFi but active on ESP-NOW
  sendEspNowBroadcast(2, "offline_wifi_espnow_failover");
}

// Gateway received data handler (Bridges ESP-NOW -> MQTT)
#if defined(ESP32)
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    inline void onDataRecv(const esp_now_recv_info_t * recv_info, const uint8_t *incomingData, int len) {
      if (len < (int)sizeof(EspNowPacket)) return;
      const EspNowPacket* packet = (const EspNowPacket*)incomingData;
  #else
    inline void onDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
      if (len < (int)sizeof(EspNowPacket)) return;
      const EspNowPacket* packet = (const EspNowPacket*)incomingData;
  #endif
#elif defined(ESP8266)
  inline void onDataRecv(uint8_t *mac_addr, uint8_t *incomingData, uint8_t len) {
    if (len < sizeof(EspNowPacket)) return;
    const EspNowPacket* packet = (const EspNowPacket*)incomingData;
  #endif

  // Only act as Gateway Bridge if explicitly configured
  if (!globalConfig.is_gateway) return;

  // Reconstruct cleaned MAC string of the client node
  char clientMacStr[13];
  snprintf(clientMacStr, sizeof(clientMacStr), "%02X%02X%02X%02X%02X%02X",
           packet->mac[0], packet->mac[1], packet->mac[2], 
           packet->mac[3], packet->mac[4], packet->mac[5]);
  
  String clientMac = String(clientMacStr);
  
  // Route to correct MQTT topics
  String topic = "";
  if (packet->type == 0) {
    topic = "cihaz/kayit/" + clientMac;
  } else if (packet->type == 1) {
    topic = "cihaz/rapor/" + clientMac;
  } else if (packet->type == 2) {
    topic = "cihaz/durum/" + clientMac;
  }

  // Publish to Broker
  if (topic.length() > 0 && mqttClient.connected()) {
    Serial.printf("[Gateway Bridge] Forwarding ESP-NOW packet from %s -> MQTT Topic: %s\n", clientMac.c_str(), topic.c_str());
    mqttClient.publish(topic.c_str(), packet->payload, false);
  }
}

// Send Status callbacks for diagnostics
#if defined(ESP32)
  inline void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // Serial.printf("[ESP-NOW] Send status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
  }
#elif defined(ESP8266)
  inline void onDataSent(uint8_t *mac_addr, uint8_t status) {
    // Serial.printf("[ESP-NOW] Send status: %s\n", status == 0 ? "Success" : "Fail");
  }
#endif

#endif
