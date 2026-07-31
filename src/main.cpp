#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <XteinkDetect.h>
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

const char* xteinkVerdictName(freeink::XteinkVerdict verdict) {
  switch (verdict) {
    case freeink::XteinkVerdict::X3Confirmed:
      return "X3 confirmed";
    case freeink::XteinkVerdict::X4Confirmed:
      return "not X3 (X4 fingerprint)";
    case freeink::XteinkVerdict::Inconclusive:
      return "inconclusive";
    default:
      return "unrecognized";
  }
}

const char* displayControllerName(BoardConfig::DisplayController controller) {
  switch (controller) {
    case BoardConfig::DisplayController::UC8253:
      return "UC8253";
    case BoardConfig::DisplayController::UC8279:
      return "UC8279d";
    default:
      return "unsupported";
  }
}

void drawBlackPixel(uint8_t* framebuffer, uint16_t widthBytes, uint16_t height, uint16_t x, uint16_t y) {
  if (framebuffer == nullptr || x >= widthBytes * 8 || y >= height) return;
  framebuffer[static_cast<uint32_t>(y) * widthBytes + x / 8] &= static_cast<uint8_t>(~(0x80U >> (x % 8)));
}

void fillBlackRect(uint8_t* framebuffer, uint16_t widthBytes, uint16_t height, uint16_t x, uint16_t y, uint16_t w,
                   uint16_t h) {
  const uint16_t displayWidth = static_cast<uint16_t>(widthBytes * 8);
  const uint16_t xCandidate = static_cast<uint16_t>(x + w);
  const uint16_t yCandidate = static_cast<uint16_t>(y + h);
  const uint16_t xEnd = xCandidate < displayWidth ? xCandidate : displayWidth;
  const uint16_t yEnd = yCandidate < height ? yCandidate : height;
  for (uint16_t py = y; py < yEnd; ++py) {
    for (uint16_t px = x; px < xEnd; ++px) drawBlackPixel(framebuffer, widthBytes, height, px, py);
  }
}

void renderDisplayTestPattern(EInkDisplay& display) {
  display.clearScreen();

  uint8_t* const framebuffer = display.getFrameBuffer();
  const uint16_t width = display.getDisplayWidth();
  const uint16_t height = display.getDisplayHeight();
  const uint16_t widthBytes = display.getDisplayWidthBytes();
  constexpr uint16_t border = 8;
  constexpr uint16_t marker = 72;
  constexpr uint16_t margin = 24;

  // Thick frame and center cross expose clipping, row stride, and geometry.
  fillBlackRect(framebuffer, widthBytes, height, 0, 0, width, border);
  fillBlackRect(framebuffer, widthBytes, height, 0, height - border, width, border);
  fillBlackRect(framebuffer, widthBytes, height, 0, 0, border, height);
  fillBlackRect(framebuffer, widthBytes, height, width - border, 0, border, height);
  fillBlackRect(framebuffer, widthBytes, height, width / 2 - 2, border, 4, height - border * 2);
  fillBlackRect(framebuffer, widthBytes, height, border, height / 2 - 2, width - border * 2, 4);

  // Four deliberately different corner markers make rotation/mirroring clear.
  fillBlackRect(framebuffer, widthBytes, height, margin, margin, marker, marker);

  for (uint16_t inset = 0; inset < 24; inset += 8) {
    const uint16_t side = marker - inset * 2;
    fillBlackRect(framebuffer, widthBytes, height, width - margin - marker + inset, margin + inset, side, 4);
    fillBlackRect(framebuffer, widthBytes, height, width - margin - marker + inset, margin + inset, 4, side);
    fillBlackRect(framebuffer, widthBytes, height, width - margin - marker + inset, margin + marker - inset - 4, side,
                  4);
    fillBlackRect(framebuffer, widthBytes, height, width - margin - inset - 4, margin + inset, 4, side);
  }

  for (uint16_t stripe = 0; stripe < marker; stripe += 16) {
    fillBlackRect(framebuffer, widthBytes, height, margin + stripe, height - margin - marker, 8, marker);
  }

  constexpr uint16_t cell = 12;
  for (uint16_t row = 0; row < marker / cell; ++row) {
    for (uint16_t col = 0; col < marker / cell; ++col) {
      if ((row + col) % 2 == 0) {
        fillBlackRect(framebuffer, widthBytes, height, width - margin - marker + col * cell,
                      height - margin - marker + row * cell, cell, cell);
      }
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(750);

  const esp_reset_reason_t resetReason = esp_reset_reason();
  Serial.printf("TamaInk %s\n", TAMAINK_VERSION);
  Serial.printf("Reset reason: %s (%d)\n", resetReasonName(resetReason), static_cast<int>(resetReason));

  uint8_t detectionScore1 = 0;
  uint8_t detectionScore2 = 0;
  const freeink::XteinkVerdict boardVerdict = freeink::detectXteinkVerdict(&detectionScore1, &detectionScore2);
  Serial.printf("Board detection: %s (I2C scores %u/%u)\n", xteinkVerdictName(boardVerdict), detectionScore1,
                detectionScore2);

  if (boardVerdict == freeink::XteinkVerdict::X3Confirmed) {
    freeink::applyXteinkDisplayController();
    Serial.printf("Display controller: %s\n", displayControllerName(BoardConfig::ACTIVE.displayController));

    const auto& pins = BoardConfig::ACTIVE.display;
    static EInkDisplay display(pins.sclk, pins.mosi, pins.cs, pins.dc, pins.rst, pins.busy);
    display.setDisplayX3();
    display.begin();
    if (!display.framebufferReady()) {
      Serial.println("Display test aborted: framebuffer allocation failed.");
      display.deepSleep();
      return;
    }

    Serial.printf("Display geometry: %ux%u, framebuffer %lu bytes\n", display.getDisplayWidth(),
                  display.getDisplayHeight(), static_cast<unsigned long>(display.getBufferSize()));
    renderDisplayTestPattern(display);
    Serial.println("Display test: starting one full refresh.");
    display.displayBuffer(EInkDisplay::FULL_REFRESH, true);
    display.deepSleep();
    Serial.println("Display test: full refresh complete; panel sleeping.");
  } else {
    Serial.println("Board detection stopped; display pins untouched.");
  }
}

void loop() { delay(1000); }
