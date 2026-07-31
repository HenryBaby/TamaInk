#include <Arduino.h>
#include <esp_system.h>

#ifndef TAMAINK_VERSION
#define TAMAINK_VERSION "unknown"
#endif

namespace {

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:
      return "unknown";
    case ESP_RST_POWERON:
      return "power-on";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "interrupt-watchdog";
    case ESP_RST_TASK_WDT:
      return "task-watchdog";
    case ESP_RST_WDT:
      return "other-watchdog";
    case ESP_RST_DEEPSLEEP:
      return "deep-sleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
    default:
      return "unrecognized";
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(750);

  const esp_reset_reason_t resetReason = esp_reset_reason();
  Serial.printf("TamaInk %s\n", TAMAINK_VERSION);
  Serial.printf("Reset reason: %s (%d)\n", resetReasonName(resetReason), static_cast<int>(resetReason));
  Serial.println("Foundation firmware ready; no hardware drivers initialized.");
}

void loop() { delay(1000); }

