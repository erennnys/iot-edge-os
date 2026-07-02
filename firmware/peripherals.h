#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <AccelStepper.h>
#include "config.h"
#include "wifi_mqtt.h"
#include "esp_now_bridge.h"

#if defined(ESP8266)
  #include <Servo.h>
#endif

// Max limits to prevent dynamic allocation fragmentation
const uint8_t MAX_CONFIGURED_PINS = 16;
const uint8_t MAX_SERVOS = 4;
const uint8_t MAX_STEPPERS = 2;
const uint8_t MAX_DHTS = 4;
const uint8_t MAX_DSS = 4;

// Custom Standalone DHT22 Reader (No external Adafruit DHT library needed)
class StandaloneDHT22 {
private:
  uint8_t pin;
  unsigned long lastReadTime;
  float temp;
  float hum;
public:
  StandaloneDHT22() : pin(255), lastReadTime(0), temp(NAN), hum(NAN) {}
  void init(uint8_t pinNum) {
    pin = pinNum;
    pinMode(pin, INPUT_PULLUP);
  }
  bool read() {
    if (pin == 255) return false;
    unsigned long now = millis();
    if (now - lastReadTime < 5000 && lastReadTime != 0) return true; // Max read interval: 5s
    lastReadTime = now;

    uint8_t data[5] = {0,0,0,0,0};
    
    // Start signal
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(20);
    digitalWrite(pin, HIGH);
    delayMicroseconds(40);
    pinMode(pin, INPUT_PULLUP);

    // Response handshake
    unsigned int timeout = 10000;
    while (digitalRead(pin) == HIGH) { if (--timeout == 0) return false; }
    timeout = 10000;
    while (digitalRead(pin) == LOW) { if (--timeout == 0) return false; }
    timeout = 10000;
    while (digitalRead(pin) == HIGH) { if (--timeout == 0) return false; }

    // Read 40 bits
    for (int i = 0; i < 40; i++) {
      timeout = 10000;
      while (digitalRead(pin) == LOW) { if (--timeout == 0) return false; }
      unsigned long t = micros();
      timeout = 10000;
      while (digitalRead(pin) == HIGH) { if (--timeout == 0) return false; }
      if ((micros() - t) > 40) {
        data[i/8] |= (1 << (7 - (i%8)));
      }
    }

    // Verify Checksum
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
      return false;
    }

    // Decode DHT22 parameters
    hum = (float)((data[0] << 8) + data[1]) * 0.1f;
    float t = (float)(((data[2] & 0x7F) << 8) + data[3]) * 0.1f;
    if (data[2] & 0x80) t = -t;
    temp = t;
    return true;
  }
  float getTemp() const { return temp; }
  float getHum() const { return hum; }
};

// ESP32 Hardware LEDC Servo Wrapper
#if defined(ESP32)
class ESP32ServoLEDC {
private:
  uint8_t pin;
  uint8_t channel;
public:
  ESP32ServoLEDC() : pin(255), channel(0) {}
  void attach(uint8_t pinNum, uint8_t chan) {
    pin = pinNum;
    channel = chan;
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcAttach(pin, 50, 16); // 50Hz, 16-bit
    #else
      ledcSetup(channel, 50, 16);
      ledcAttachPin(pin, channel);
    #endif
  }
  void write(int angle) {
    if (pin == 255) return;
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    
    // Duty cycles corresponding to 0.5ms (0 deg) to 2.5ms (180 deg) pulse width
    int duty = 1638 + (angle * (8192 - 1638) / 180);
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcWrite(pin, duty);
    #else
      ledcWrite(channel, duty);
    #endif
  }
};
#endif

// Struct to represent a runtime pin configuration
struct PinConfigEntry {
  uint8_t gpioPins[4]; // Array to support step motor (4 pins). Single pin uses index 0.
  uint8_t pinCount = 1;
  char mode[4] = ""; // "DO", "DI", "AI", "PWM", "SRV", "STM", "DHT", "DS"
  
  // Runtime State
  bool lastState = false;
  bool currentState = false;
  unsigned long lastDebounceTime = 0;
  
  int lastAnalogVal = -1;
  unsigned long lastSensorReadTime = 0;
  
  // Peripheral indices
  int8_t resourceIndex = -1; 
};

class PinManager {
private:
  PinConfigEntry configuredPins[MAX_CONFIGURED_PINS];
  uint8_t configuredPinCount = 0;

  // Static allocation of peripheral objects
  #if defined(ESP8266)
    Servo servos[MAX_SERVOS];
  #elif defined(ESP32)
    ESP32ServoLEDC servos[MAX_SERVOS];
  #endif
  uint8_t servoCount = 0;

  AccelStepper* steppers[MAX_STEPPERS];
  uint8_t stepperCount = 0;

  StandaloneDHT22 dhts[MAX_DHTS];
  uint8_t dhtCount = 0;

  OneWire* oneWires[MAX_DSS];
  DallasTemperature* dsSensors[MAX_DSS];
  uint8_t dsCount = 0;

  bool serialClosed = false;

  // Publish report to active connection channel (MQTT or ESP-NOW)
  void sendTelemetry(const char* key, float value) {
    char valStr[16];
    dtostrf(value, 4, 2, valStr);
    sendTelemetryString(key, valStr);
  }

  void sendTelemetryString(const char* key, const char* value) {
    // Keep JSON minimal
    char jsonPayload[64];
    snprintf(jsonPayload, sizeof(jsonPayload), "{\"%s\":\"%s\"}", key, value);

    if (currentNetState == STATE_CONNECTED && mqttClient.connected()) {
      String topic = "cihaz/rapor/" + cleanedMac;
      mqttClient.publish(topic.c_str(), jsonPayload, false);
    } else if (currentNetState == STATE_ESPNOW_ONLY) {
      sendEspNowBroadcast(1, jsonPayload); // 1 = report (rapor)
    }
  }

  // Verify and shut down Serial if RX/TX are used as General I/O (ESP-01 safety)
  void checkSerialOverride(uint8_t pin) {
    if (serialClosed) return;
    if (pin == 1 || pin == 3) { // TX (1) or RX (3)
      Serial.println(F("[System] Warning: RX/TX pins configured for I/O! Shutting down Serial interface..."));
      Serial.flush();
      Serial.end();
      serialClosed = true;
    }
  }

public:
  PinManager() : configuredPinCount(0), servoCount(0), stepperCount(0), dhtCount(0), dsCount(0) {}

  void loadAndSetupPeripherals() {
    if (!FS_SYSTEM.exists(F("/pins.json"))) {
      Serial.println(F("[Pins] No pins.json configuration."));
      return;
    }

    File pinFile = FS_SYSTEM.open(F("/pins.json"), "r");
    if (!pinFile) {
      Serial.println(F("[Pins] Failed to open pins.json"));
      return;
    }

    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, pinFile);
    pinFile.close();

    if (error) {
      Serial.println(F("[Pins] JSON parse error on pins.json"));
      return;
    }

    // Clear previous settings
    configuredPinCount = 0;
    servoCount = 0;
    dhtCount = 0;
    dsCount = 0;
    
    // Clean Steppers
    for (int i = 0; i < stepperCount; i++) {
      delete steppers[i];
    }
    stepperCount = 0;

    // Clean DS18B20 resources
    for (int i = 0; i < dsCount; i++) {
      delete dsSensors[i];
      delete oneWires[i];
    }
    dsCount = 0;

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
      if (configuredPinCount >= MAX_CONFIGURED_PINS) break;

      PinConfigEntry& entry = configuredPins[configuredPinCount];
      strlcpy(entry.mode, obj["m"] | "", sizeof(entry.mode));
      
      // Parse pin number (could be integer or array for steppers)
      if (obj["p"].is<JsonArray>()) {
        JsonArray pinArr = obj["p"].as<JsonArray>();
        entry.pinCount = pinArr.size();
        for (size_t i = 0; i < entry.pinCount && i < 4; i++) {
          entry.gpioPins[i] = pinArr[i];
          checkSerialOverride(entry.gpioPins[i]);
        }
      } else {
        entry.gpioPins[0] = obj["p"] | 0;
        entry.pinCount = 1;
        checkSerialOverride(entry.gpioPins[0]);
      }

      // Initialize Pin Modes
      if (strcmp(entry.mode, "DO") == 0) {
        pinMode(entry.gpioPins[0], OUTPUT);
        digitalWrite(entry.gpioPins[0], LOW);
      } 
      else if (strcmp(entry.mode, "DI") == 0) {
        pinMode(entry.gpioPins[0], INPUT_PULLUP);
        entry.currentState = digitalRead(entry.gpioPins[0]);
        entry.lastState = entry.currentState;
        entry.lastDebounceTime = millis();
      } 
      else if (strcmp(entry.mode, "AI") == 0) {
        pinMode(entry.gpioPins[0], INPUT);
        entry.lastAnalogVal = analogRead(entry.gpioPins[0]);
        entry.lastSensorReadTime = millis();
      }
      else if (strcmp(entry.mode, "PWM") == 0) {
        pinMode(entry.gpioPins[0], OUTPUT);
        #if defined(ESP8266)
          analogWrite(entry.gpioPins[0], 0);
        #elif defined(ESP32)
          #if ESP_ARDUINO_VERSION_MAJOR >= 3
            ledcAttach(entry.gpioPins[0], 5000, 8); // 5kHz, 8-bit
            ledcWrite(entry.gpioPins[0], 0);
          #else
            ledcSetup(configuredPinCount, 5000, 8); // use pin index as channel
            ledcAttachPin(entry.gpioPins[0], configuredPinCount);
            ledcWrite(configuredPinCount, 0);
          #endif
        #endif
      }
      else if (strcmp(entry.mode, "SRV") == 0) {
        if (servoCount < MAX_SERVOS) {
          #if defined(ESP8266)
            servos[servoCount].attach(entry.gpioPins[0]);
          #elif defined(ESP32)
            servos[servoCount].attach(entry.gpioPins[0], servoCount);
          #endif
          entry.resourceIndex = servoCount;
          servoCount++;
        }
      }
      else if (strcmp(entry.mode, "STM") == 0) {
        // Step Motor configuration needs 4 pins
        if (entry.pinCount >= 4 && stepperCount < MAX_STEPPERS) {
          steppers[stepperCount] = new AccelStepper(
            AccelStepper::HALF4WIRE,
            entry.gpioPins[0], entry.gpioPins[2], // Pin 1, Pin 3
            entry.gpioPins[1], entry.gpioPins[3]  // Pin 2, Pin 4
          );
          steppers[stepperCount]->setMaxSpeed(1000.0);
          steppers[stepperCount]->setAcceleration(500.0);
          entry.resourceIndex = stepperCount;
          stepperCount++;
        }
      }
      else if (strcmp(entry.mode, "DHT") == 0) {
        if (dhtCount < MAX_DHTS) {
          dhts[dhtCount].init(entry.gpioPins[0]);
          entry.resourceIndex = dhtCount;
          dhtCount++;
          entry.lastSensorReadTime = millis();
        }
      }
      else if (strcmp(entry.mode, "DS") == 0) {
        if (dsCount < MAX_DSS) {
          oneWires[dsCount] = new OneWire(entry.gpioPins[0]);
          dsSensors[dsCount] = new DallasTemperature(oneWires[dsCount]);
          dsSensors[dsCount]->begin();
          entry.resourceIndex = dsCount;
          dsCount++;
          entry.lastSensorReadTime = millis();
        }
      }

      configuredPinCount++;
    }
    
    if (!serialClosed) {
      Serial.printf("[Pins] Dynamic pin assignment completed. Active resources: Pins=%d, Servos=%d, Steppers=%d, DHTs=%d, DS18B20s=%d\n",
                    configuredPinCount, servoCount, stepperCount, dhtCount, dsCount);
    }
  }

  // Handle incoming MQTT / ESP-NOW control actions
  void handleWriteAction(uint8_t targetPin, const char* actionValue) {
    for (uint8_t i = 0; i < configuredPinCount; i++) {
      PinConfigEntry& entry = configuredPins[i];
      if (entry.gpioPins[0] == targetPin) {
        
        if (strcmp(entry.mode, "DO") == 0) {
          int val = atoi(actionValue);
          digitalWrite(targetPin, val ? HIGH : LOW);
          sendTelemetryString(String("GPIO_" + String(targetPin)).c_str(), val ? "1" : "0");
        }
        else if (strcmp(entry.mode, "PWM") == 0) {
          int val = atoi(actionValue);
          if (val < 0) val = 0;
          if (val > 255) val = 255;
          
          #if defined(ESP8266)
            analogWrite(targetPin, val * 4); // scale to 1023
          #elif defined(ESP32)
            #if ESP_ARDUINO_VERSION_MAJOR >= 3
              ledcWrite(targetPin, val);
            #else
              ledcWrite(i, val); // use pin configuration index as channel
            #endif
          #endif
          
          char valStr[8];
          itoa(val, valStr, 10);
          sendTelemetryString(String("PWM_" + String(targetPin)).c_str(), valStr);
        }
        else if (strcmp(entry.mode, "SRV") == 0 && entry.resourceIndex != -1) {
          int angle = atoi(actionValue);
          servos[entry.resourceIndex].write(angle);
          
          char angleStr[8];
          itoa(angle, angleStr, 10);
          sendTelemetryString(String("SRV_" + String(targetPin)).c_str(), angleStr);
        }
        else if (strcmp(entry.mode, "STM") == 0 && entry.resourceIndex != -1) {
          long steps = atol(actionValue);
          steppers[entry.resourceIndex]->move(steps);
        }
        break;
      }
    }
  }

  // Periodic polling run function
  void run() {
    unsigned long now = millis();

    // 1. Step Motor running loop (must execute frequently)
    for (uint8_t i = 0; i < stepperCount; i++) {
      if (steppers[i]->distanceToGo() != 0) {
        steppers[i]->run();
        
        // Anti-starvation yield to prevent MQTT timeouts during large step runs
        #if defined(ESP8266)
          static unsigned long lastYield = 0;
          if (now - lastYield > 50) {
            lastYield = now;
            yield();
            mqttClient.loop();
          }
        #endif
      }
    }

    // 2. Poll Pin Inputs & Sensors
    for (uint8_t i = 0; i < configuredPinCount; i++) {
      PinConfigEntry& entry = configuredPins[i];

      if (strcmp(entry.mode, "DI") == 0) {
        // Asynchronous software debounce (50ms filter)
        bool readVal = digitalRead(entry.gpioPins[0]);
        if (readVal != entry.lastState) {
          entry.lastDebounceTime = now;
          entry.lastState = readVal;
        }

        if ((now - entry.lastDebounceTime) > 50) {
          if (readVal != entry.currentState) {
            entry.currentState = readVal;
            char keyName[16];
            snprintf(keyName, sizeof(keyName), "GPIO_%d", entry.gpioPins[0]);
            sendTelemetryString(keyName, readVal ? "1" : "0");
            
            // Trigger offline local rules evaluation
            extern void evaluateLocalRules(uint8_t pin, const char* value);
            evaluateLocalRules(entry.gpioPins[0], readVal ? "1" : "0");
          }
        }
      }
      else if (strcmp(entry.mode, "AI") == 0) {
        if (now - entry.lastSensorReadTime > 5000) { // read analog every 5s
          entry.lastSensorReadTime = now;
          int val = analogRead(entry.gpioPins[0]);
          // Deadband filter: send only if it changes by more than 5
          if (abs(val - entry.lastAnalogVal) > 5) {
            entry.lastAnalogVal = val;
            char keyName[16];
            snprintf(keyName, sizeof(keyName), "ADC_%d", entry.gpioPins[0]);
            char valStr[8];
            itoa(val, valStr, 10);
            sendTelemetryString(keyName, valStr);
            
            // Trigger offline local rules evaluation
            extern void evaluateLocalRules(uint8_t pin, const char* value);
            evaluateLocalRules(entry.gpioPins[0], valStr);
          }
        }
      }
      else if (strcmp(entry.mode, "DHT") == 0 && entry.resourceIndex != -1) {
        if (now - entry.lastSensorReadTime > 10000) { // read DHT every 10s
          entry.lastSensorReadTime = now;
          if (dhts[entry.resourceIndex].read()) {
            float temp = dhts[entry.resourceIndex].getTemp();
            float hum = dhts[entry.resourceIndex].getHum();
            
            if (!isnan(temp)) {
              char keyName[20];
              snprintf(keyName, sizeof(keyName), "DHT_T_%d", entry.gpioPins[0]);
              sendTelemetry(keyName, temp);
            }
            if (!isnan(hum)) {
              char keyName[20];
              snprintf(keyName, sizeof(keyName), "DHT_H_%d", entry.gpioPins[0]);
              sendTelemetry(keyName, hum);
            }
          }
        }
      }
      else if (strcmp(entry.mode, "DS") == 0 && entry.resourceIndex != -1) {
        if (now - entry.lastSensorReadTime > 10000) { // read DS18B20 every 10s
          entry.lastSensorReadTime = now;
          dsSensors[entry.resourceIndex]->requestTemperatures();
          float temp = dsSensors[entry.resourceIndex]->getTempCByIndex(0);
          
          if (temp != DEVICE_DISCONNECTED_C) {
            char keyName[20];
            snprintf(keyName, sizeof(keyName), "DS_T_%d", entry.gpioPins[0]);
            sendTelemetry(keyName, temp);
          }
        }
      }
    }
  }
};

extern PinManager pinManager;

#endif
