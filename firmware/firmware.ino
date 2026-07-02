#include <Arduino.h>
#include "config.h"
#include "wifi_mqtt.h"
#include "esp_now_bridge.h"
#include "peripherals.h"
#include "rules_engine.h"

// Define Global variables declared in headers
DeviceConfig globalConfig;
String cleanedMac;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
DNSServer dnsServer;
WebServerClass* server = nullptr;
NetworkState currentNetState = STATE_CONNECTING;
unsigned long lastWifiRetryTime = 0;
uint8_t wifiRetryCount = 0;
uint8_t activeChannel = 1;
bool espNowActive = false;

// Peripherals and Rules engine instances
PinManager pinManager;
LocalRulesEngine localRulesEngine;

// Link functions used by wifi_mqtt.h callbacks
void reloadPeripheralsDynamic() {
  pinManager.loadAndSetupPeripherals();
}
void reloadRulesDynamic() {
  localRulesEngine.loadLocalRules();
}
void handlePeripheralAction(uint8_t pin, const char* value) {
  pinManager.handleWriteAction(pin, value);
}
void evaluateLocalRules(uint8_t pin, const char* value) {
  localRulesEngine.evaluate(pin, value);
}

// FreeRTOS Task and Watchdog Definitions for ESP32
#if defined(ESP32)
  #include <esp_task_wdt.h>
  #define WDT_TIMEOUT_S 4
  
  TaskHandle_t TaskNetHandle = NULL;
  TaskHandle_t TaskActHandle = NULL;

  void TaskNetworkCode(void * pvParameters) {
    Serial.printf("[FreeRTOS] TaskNetwork (Core %d) started.\n", xPortGetCoreID());
    
    // Add this task to Task Watchdog
    esp_task_wdt_add(NULL);

    for(;;) {
      checkNetworkLoop();
      esp_task_wdt_reset(); // Feed WDT
      vTaskDelay(pdMS_TO_TICKS(10)); // Yield to CPU
    }
  }

  void TaskActuatorsCode(void * pvParameters) {
    Serial.printf("[FreeRTOS] TaskActuators (Core %d) started.\n", xPortGetCoreID());
    
    // Add actuator task to Watchdog
    esp_task_wdt_add(NULL);

    for(;;) {
      pinManager.run();
      esp_task_wdt_reset(); // Feed WDT
      vTaskDelay(pdMS_TO_TICKS(1)); // Run fast for stepper motor responsiveness
    }
  }
#endif

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n--- IoT OS Unified Firmware Starting ---"));
  Serial.printf("Target Board: %s\n", BOARD_TYPE);

  // Initialize LittleFS
  if (!initFS()) {
    Serial.println(F("[System] File system failure. Rebooting..."));
    delay(3000);
    ESP.restart();
  }

  // Get and clean MAC Address
  cleanedMac = getCleanedMac();
  Serial.printf("[System] Device Cleaned MAC: %s\n", cleanedMac.c_str());

  // Load Configuration
  loadConfig();

  // Load Peripherals dynamic configuration
  pinManager.loadAndSetupPeripherals();

  // Load local offline rules configuration
  localRulesEngine.loadLocalRules();

  // Watchdog initialization
  #if defined(ESP32)
    Serial.println(F("[WDT] Initializing Hardware Task Watchdog..."));
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
      esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_S * 1000,
        .idle_core_mask = (1 << 0) | (1 << 1), // Monitor both cores
        .trigger_panic = true
      };
      esp_task_wdt_init(&wdt_config);
    #else
      esp_task_wdt_init(WDT_TIMEOUT_S, true);
    #endif
  #elif defined(ESP8266)
    Serial.println(F("[WDT] Enabling ESP8266 Hardware Watchdog..."));
    ESP.wdtEnable(WDTO_4S);
  #endif

  // Network Startup Sequence
  if (globalConfig.config_version == 0) {
    Serial.println(F("[System] First run or missing configuration. Launching AP Config Portal..."));
    startAPMode();
  } else {
    // If not first run, start WiFi client
    connectWiFi();
  }

  // Task scheduling for ESP32 Dual Core
  #if defined(ESP32)
    Serial.println(F("[System] Creating FreeRTOS Tasks..."));
    
    // Core 0: Network management
    xTaskCreatePinnedToCore(
      TaskNetworkCode,
      "TaskNetwork",
      8192,
      NULL,
      1,
      &TaskNetHandle,
      0
    );

    // Core 1: Actuators, sensors, local rules
    xTaskCreatePinnedToCore(
      TaskActuatorsCode,
      "TaskActuators",
      8192,
      NULL,
      1,
      &TaskActHandle,
      1
    );
  #endif
}

void loop() {
  #if !defined(ESP32)
    // ESP8266 Single-Core Async millis Loop (No FreeRTOS)
    checkNetworkLoop();
    pinManager.run();
    ESP.wdtFeed(); // Feed Hardware Watchdog
    yield();       // Prevent ESP8266 hardware crash
  #else
    // ESP32 uses FreeRTOS tasks, so the main loop thread can be suspended or run low-priority checks
    vTaskDelay(pdMS_TO_TICKS(1000));
  #endif
}
