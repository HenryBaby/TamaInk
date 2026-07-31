#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <SDCardManager.h>
#include <SPI.h>
#include <XteinkDetect.h>
#include <esp_system.h>
#include <cstring>

#ifndef TAMAINK_VERSION
#define TAMAINK_VERSION "unknown"
#endif

namespace {

constexpr uint8_t BUTTON_COUNT = InputManager::BTN_POWER + 1;
constexpr unsigned long INPUT_REPOLL_MS = 6;
constexpr unsigned long HOLD_REPORT_INTERVAL_MS = 1000;

InputManager inputManager;
bool inputReady = false;
uint8_t lastInputState = 0;
unsigned long pressStartedAt[BUTTON_COUNT] = {};
unsigned long lastHoldReportAt[BUTTON_COUNT] = {};

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

void drawWhitePixel(uint8_t* framebuffer, uint16_t widthBytes, uint16_t height, uint16_t x, uint16_t y) {
  if (framebuffer == nullptr || x >= widthBytes * 8 || y >= height) return;
  framebuffer[static_cast<uint32_t>(y) * widthBytes + x / 8] |= static_cast<uint8_t>(0x80U >> (x % 8));
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

void fillWhiteRect(uint8_t* framebuffer, uint16_t widthBytes, uint16_t height, uint16_t x, uint16_t y, uint16_t w,
                   uint16_t h) {
  const uint16_t displayWidth = static_cast<uint16_t>(widthBytes * 8);
  const uint16_t xCandidate = static_cast<uint16_t>(x + w);
  const uint16_t yCandidate = static_cast<uint16_t>(y + h);
  const uint16_t xEnd = xCandidate < displayWidth ? xCandidate : displayWidth;
  const uint16_t yEnd = yCandidate < height ? yCandidate : height;
  for (uint16_t py = y; py < yEnd; ++py) {
    for (uint16_t px = x; px < xEnd; ++px) drawWhitePixel(framebuffer, widthBytes, height, px, py);
  }
}

void renderTransitionTarget(EInkDisplay& display, bool rightHalfBlack) {
  uint8_t* const framebuffer = display.getFrameBuffer();
  const uint16_t widthBytes = display.getDisplayWidthBytes();
  const uint16_t height = display.getDisplayHeight();
  constexpr uint16_t targetWidth = 192;
  constexpr uint16_t targetHeight = 96;
  constexpr uint16_t targetBorder = 8;
  const uint16_t x = static_cast<uint16_t>((display.getDisplayWidth() - targetWidth) / 2);
  const uint16_t y = static_cast<uint16_t>(height / 2 - targetHeight - 32);

  fillWhiteRect(framebuffer, widthBytes, height, x, y, targetWidth, targetHeight);
  fillBlackRect(framebuffer, widthBytes, height, x, y, targetWidth, targetBorder);
  fillBlackRect(framebuffer, widthBytes, height, x, y + targetHeight - targetBorder, targetWidth, targetBorder);
  fillBlackRect(framebuffer, widthBytes, height, x, y, targetBorder, targetHeight);
  fillBlackRect(framebuffer, widthBytes, height, x + targetWidth - targetBorder, y, targetBorder, targetHeight);

  const uint16_t innerX = static_cast<uint16_t>(x + targetBorder);
  const uint16_t innerY = static_cast<uint16_t>(y + targetBorder);
  const uint16_t innerWidth = static_cast<uint16_t>(targetWidth - targetBorder * 2);
  const uint16_t halfWidth = static_cast<uint16_t>(innerWidth / 2);
  const uint16_t blackX = rightHalfBlack ? static_cast<uint16_t>(innerX + halfWidth) : innerX;
  fillBlackRect(framebuffer, widthBytes, height, blackX, innerY, halfWidth,
                static_cast<uint16_t>(targetHeight - targetBorder * 2));
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

const char* classifiedButtonName(int button) {
  return button >= 0 && button < BUTTON_COUNT ? InputManager::getButtonName(static_cast<uint8_t>(button)) : "None";
}

uint8_t currentInputState() {
  uint8_t state = 0;
  for (uint8_t button = 0; button < BUTTON_COUNT; ++button) {
    if (inputManager.isPressed(button)) state |= static_cast<uint8_t>(1U << button);
  }
  return state;
}

void printAdcSnapshot() {
  InputManager::ButtonAdcSample group1 = {};
  InputManager::ButtonAdcSample group2 = {};
  inputManager.readButtonAdc(group1, group2);
  Serial.printf("Input ADC: group1 GPIO%d raw=%d classified=%s; group2 GPIO%d raw=%d classified=%s\n", group1.pin,
                group1.raw, classifiedButtonName(group1.button), group2.pin, group2.raw,
                classifiedButtonName(group2.button));
}

bool initializeX3SharedSpi() {
  const auto& displayPins = BoardConfig::ACTIVE.display;
  const auto& sdPins = BoardConfig::ACTIVE.sd;
  if (displayPins.sclk < 0 || displayPins.mosi < 0 || displayPins.cs < 0 || sdPins.miso < 0 || sdPins.cs < 0) {
    Serial.printf("X3 SPI init skipped: invalid pins (display sclk=%d mosi=%d cs=%d; SD miso=%d cs=%d)\n",
                  displayPins.sclk, displayPins.mosi, displayPins.cs, sdPins.miso, sdPins.cs);
    return false;
  }

  // Keep the SD card deselected before any panel traffic, then initialize the
  // shared bus with MISO attached so later SPI.begin() calls remain compatible.
  pinMode(sdPins.cs, OUTPUT);
  digitalWrite(sdPins.cs, HIGH);
  if (!SPI.begin(displayPins.sclk, sdPins.miso, displayPins.mosi, displayPins.cs)) {
    SPI.end();
    Serial.println("X3 shared SPI init failed");
    return false;
  }
  Serial.printf("X3 shared SPI ready: sclk=%d miso=%d mosi=%d displayCS=%d sdCS=%d\n", displayPins.sclk,
                sdPins.miso, displayPins.mosi, displayPins.cs, sdPins.cs);
  return true;
}

void beginInputDiagnostic() {
  inputManager.begin();
  inputManager.update();
  delay(10);
  inputManager.update();
  lastInputState = currentInputState();
  const unsigned long now = millis();
  for (uint8_t button = 0; button < BUTTON_COUNT; ++button) {
    if (inputManager.isPressed(button)) {
      pressStartedAt[button] = now;
      lastHoldReportAt[button] = 0;
      Serial.printf("Input INITIAL: %s already pressed\n", InputManager::getButtonName(button));
    }
  }
  Serial.printf("Input STATE: 0x%02X%s\n", lastInputState,
                (lastInputState & (lastInputState - 1)) != 0 ? " (simultaneous)" : "");
  printAdcSnapshot();
  Serial.println("Input diagnostic ready: press, release, hold, and combine physical buttons.");
  inputReady = true;
}

void updateInputDiagnostic() {
  inputManager.update();
  if (inputManager.isDebouncePending()) {
    delay(INPUT_REPOLL_MS);
    inputManager.update();
  }

  const unsigned long now = millis();
  const uint8_t state = currentInputState();
  for (uint8_t button = 0; button < BUTTON_COUNT; ++button) {
    if (inputManager.wasPressed(button)) {
      pressStartedAt[button] = now;
      lastHoldReportAt[button] = 0;
      Serial.printf("Input PRESS: %s\n", InputManager::getButtonName(button));
    }
    if (inputManager.wasReleased(button)) {
      const unsigned long heldMs = now - pressStartedAt[button];
      Serial.printf("Input RELEASE: %s held=%lu ms\n", InputManager::getButtonName(button), heldMs);
    }
    if (inputManager.isPressed(button)) {
      const unsigned long heldMs = now - pressStartedAt[button];
      const unsigned long reportAt = heldMs / HOLD_REPORT_INTERVAL_MS;
      if (reportAt > lastHoldReportAt[button]) {
        lastHoldReportAt[button] = reportAt;
        Serial.printf("Input HOLD: %s held=%lu ms\n", InputManager::getButtonName(button), heldMs);
      }
    }
  }

  if (state != lastInputState) {
    Serial.printf("Input STATE: 0x%02X%s\n", state, (state & (state - 1)) != 0 ? " (simultaneous)" : "");
    printAdcSnapshot();
    lastInputState = state;
  }
}

void runStorageDiagnostic() {
  constexpr char sentinelPath[] = "/tamaink-readonly-test.txt";
  constexpr char sentinelContents[] = "TAMAINK_STORAGE_TEST_V1";
  constexpr size_t sentinelLength = sizeof(sentinelContents) - 1;
  char contents[sentinelLength] = {};
  Serial.printf("SD diagnostic: read-only checks; sentinel=%s\n", sentinelPath);
  if (!SdMan.begin()) {
    Serial.println("SD diagnostic: mount failed");
    return;
  }
  Serial.printf("SD diagnostic: mounted total=%llu used=%llu bytes\n",
                static_cast<unsigned long long>(SdMan.sdTotalBytes()),
                static_cast<unsigned long long>(SdMan.sdUsedBytes()));
  FsFile file = SdMan.open(sentinelPath, O_RDONLY);
  if (!file) {
    Serial.println(SdMan.exists(sentinelPath) ? "SD diagnostic: sentinel open failure"
                                              : "SD diagnostic: sentinel missing");
    return;
  }
  const uint64_t fileSize = file.size();
  if (fileSize != sentinelLength) {
    file.close();
    Serial.printf("SD diagnostic: sentinel wrong size (%llu, expected %u)\n",
                  static_cast<unsigned long long>(fileSize), static_cast<unsigned>(sentinelLength));
    return;
  }
  const int bytesRead = file.read(contents, sentinelLength);
  file.close();
  if (bytesRead < 0) {
    Serial.println("SD diagnostic: sentinel read failure");
    return;
  }
  if (static_cast<size_t>(bytesRead) != sentinelLength) {
    Serial.printf("SD diagnostic: sentinel short read (%d, expected %u)\n", bytesRead,
                  static_cast<unsigned>(sentinelLength));
    return;
  }
  if (std::memcmp(contents, sentinelContents, sentinelLength) != 0) {
    Serial.println("SD diagnostic: sentinel content mismatch");
    return;
  }
  Serial.println("SD diagnostic: sentinel pass");
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
    if (!initializeX3SharedSpi()) return;
    display.begin();
    if (!display.framebufferReady()) {
      Serial.println("Display test aborted: framebuffer allocation failed.");
      display.deepSleep();
      return;
    }

    Serial.printf("Display geometry: %ux%u, framebuffer %lu bytes\n", display.getDisplayWidth(),
                  display.getDisplayHeight(), static_cast<unsigned long>(display.getBufferSize()));
    renderDisplayTestPattern(display);
    renderTransitionTarget(display, false);
    Serial.println("Display test phase 1/3: full refresh; transition target starts with logical left half black.");
    display.displayBuffer(EInkDisplay::FULL_REFRESH, false);
    Serial.println("Display test phase 1/3 complete; holding for 3 seconds.");
    delay(3000);

    // The explicit full refresh and the driver's automatic settling pass leave
    // both controller planes synchronized, so subsequent calls can exercise
    // their requested modes instead of the conservative boot-time full resync.
    display.skipInitialResync();
    const bool hasDistinctHalfAndFast =
        BoardConfig::ACTIVE.displayController == BoardConfig::DisplayController::UC8253;

    renderTransitionTarget(display, true);
    Serial.printf("Display test phase 2/3: %s; transition target swaps to logical right half black.\n",
                  hasDistinctHalfAndFast ? "half refresh" : "differential refresh A (half/fast alias)");
    display.displayBuffer(EInkDisplay::HALF_REFRESH, false);
    Serial.println("Display test phase 2/3 complete; holding for 3 seconds.");
    delay(3000);

    renderTransitionTarget(display, false);
    Serial.printf("Display test phase 3/3: %s; transition target swaps back.\n",
                  hasDistinctHalfAndFast ? "fast differential refresh" : "differential refresh B (half/fast alias)");
    display.displayBuffer(EInkDisplay::FAST_REFRESH, true);
    display.deepSleep();
    Serial.println("Display test phase 3/3 complete; panel sleeping.");
    runStorageDiagnostic();
    beginInputDiagnostic();
  } else {
    Serial.println("Board detection stopped; display pins untouched.");
  }
}

void loop() {
  if (inputReady) updateInputDiagnostic();
  delay(10);
}
