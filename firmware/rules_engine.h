#ifndef RULES_ENGINE_H
#define RULES_ENGINE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "peripherals.h"

const uint8_t MAX_LOCAL_RULES = 16;

struct LocalRuleEntry {
  uint8_t sourcePin = 255;
  uint8_t targetPin = 255;
  char op[3] = ""; // "==", "!=", ">", "<"
  char triggerVal[16] = "";
  char actionVal[16] = "";
};

class LocalRulesEngine {
private:
  LocalRuleEntry localRules[MAX_LOCAL_RULES];
  uint8_t ruleCount = 0;

  bool evaluateCondition(const char* actualVal, const char* op, const char* targetVal) {
    if (strcmp(op, "==") == 0) {
      return strcmp(actualVal, targetVal) == 0;
    }
    else if (strcmp(op, "!=") == 0) {
      return strcmp(actualVal, targetVal) != 0;
    }
    else if (strcmp(op, ">") == 0) {
      return atof(actualVal) > atof(targetVal);
    }
    else if (strcmp(op, "<") == 0) {
      return atof(actualVal) < atof(targetVal);
    }
    return false;
  }

public:
  LocalRulesEngine() : ruleCount(0) {}

  void loadLocalRules() {
    if (!FS_SYSTEM.exists(F("/rules.json"))) {
      Serial.println(F("[Rules] No local rules.json configuration."));
      ruleCount = 0;
      return;
    }

    File rulesFile = FS_SYSTEM.open(F("/rules.json"), "r");
    if (!rulesFile) {
      Serial.println(F("[Rules] Failed to open rules.json"));
      ruleCount = 0;
      return;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, rulesFile);
    rulesFile.close();

    if (error) {
      Serial.println(F("[Rules] JSON parse error on rules.json"));
      ruleCount = 0;
      return;
    }

    ruleCount = 0;
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
      if (ruleCount >= MAX_LOCAL_RULES) break;

      LocalRuleEntry& entry = localRules[ruleCount];
      entry.sourcePin = obj["s"] | 255;
      entry.targetPin = obj["t"] | 255;
      strlcpy(entry.op, obj["o"] | "", sizeof(entry.op));
      strlcpy(entry.triggerVal, obj["v"] | "", sizeof(entry.triggerVal));
      strlcpy(entry.actionVal, obj["a"] | "", sizeof(entry.actionVal));

      ruleCount++;
    }

    Serial.printf("[Rules] Loaded %d offline local rules.\n", ruleCount);
  }

  // Trigger evaluation: evaluates if state changes of sourcePin fire any offline actions
  void evaluate(uint8_t sourcePin, const char* actualValue) {
    for (uint8_t i = 0; i < ruleCount; i++) {
      LocalRuleEntry& rule = localRules[i];
      if (rule.sourcePin == sourcePin) {
        if (evaluateCondition(actualValue, rule.op, rule.triggerVal)) {
          Serial.printf("[Rules] Local Rule Triggered! Pin %d value %s met condition '%s %s'. Writing %s to Target Pin %d\n",
                        sourcePin, actualValue, rule.op, rule.triggerVal, rule.actionVal, rule.targetPin);
          
          // Action execution offline!
          pinManager.handleWriteAction(rule.targetPin, rule.actionVal);
        }
      }
    }
  }
};

extern LocalRulesEngine localRulesEngine;

#endif
