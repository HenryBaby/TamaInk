#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <SDCardManager.h>
#include <SPI.h>
#include <XteinkDetect.h>
#include <BatteryMonitor.h>
#include <Wire.h>
#include <esp_system.h>
#include <cstring>
#include <new>
#include "tamaink_persistence.h"
#include "tamaink_rom.h"
#include "tamaink_tamalib.h"
#include "tamaink_renderer.h"
#include "tamaink_emulator_state.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#ifndef TAMAINK_VERSION
#define TAMAINK_VERSION "unknown"
#endif

namespace {

constexpr uint8_t BUTTON_COUNT = InputManager::BTN_POWER + 1;
constexpr unsigned long INPUT_REPOLL_MS = 6;
constexpr unsigned long HOLD_REPORT_INTERVAL_MS = 1000;
constexpr unsigned long EMULATOR_SERIAL_FRAME_INTERVAL_MS = 250;

InputManager inputManager;
bool inputReady = false;
bool emulatorActive = false;
tamaink::tamalib::Adapter emulator;
std::uint16_t* emulatorProgram = nullptr;
tamaink::tamalib::Snapshot emulatorSnapshot{};
tamaink::tamalib::Snapshot emulatorPrinted{};
bool emulatorPrintedValid = false;
tamaink::tamalib::Snapshot emulatorObserved{};
bool emulatorObservedValid = false;
bool serialFramePending = false;
bool serialLcdFramesEnabled = false;
bool rendererFramePending = false;
unsigned long emulatorLastPrintAt = 0;
EInkDisplay* rendererDisplay = nullptr;
QueueHandle_t rendererQueue = nullptr;
TaskHandle_t rendererTaskHandle = nullptr;
bool rendererEnabled = false;
bool rendererBegun = false;
bool initializeX3SharedSpi();
void scanPersistence();
bool readPersistenceSlot(uint8_t slot, tamaink::persist::Record& out);
bool payloadMatches(const tamaink::persist::Record& r);
extern bool persistenceHasSelected;
extern uint8_t persistenceSlot;
extern uint32_t persistenceGeneration;
extern bool persistenceIdentityReady;
extern uint8_t kPersistRom[8];
extern tamaink::persist::Record persistenceBootRecord[2];
extern tamaink::emulator::State persistenceBootState;
extern uint8_t persistenceCandidate[tamaink::persist::kHeaderSize + tamaink::persist::kMaxPayload];

void rendererTask(void*) {
  tamaink::tamalib::Snapshot frame{};
  bool first = true;
  unsigned long lastRefresh = 0;
  for (;;) {
    if (xQueueReceive(rendererQueue, &frame, portMAX_DELAY) != pdTRUE) continue;
    while (rendererDisplay->refreshBusy()) vTaskDelay(pdMS_TO_TICKS(20));
    const unsigned long now = millis();
    if (!first && now - lastRefresh < 1000) vTaskDelay(pdMS_TO_TICKS(1000 - (now - lastRefresh)));
    tamaink::tamalib::Snapshot newest{};
    while (xQueueReceive(rendererQueue, &newest, 0) == pdTRUE) frame = newest;
    const auto status = tamaink::render::snapshot(frame, rendererDisplay->getFrameBuffer(),
        rendererDisplay->getBufferSize(), rendererDisplay->getDisplayWidth(), rendererDisplay->getDisplayHeight(),
        rendererDisplay->getDisplayWidthBytes(), 268, 8, 16, tamaink::render::Rotation::CounterClockwise90,
        tamaink::render::IconLayout::P1BottomRow);
    if (status != tamaink::render::Status::Ok) { Serial.println("Display renderer: frame geometry rejected"); continue; }
    rendererDisplay->displayBuffer(first ? EInkDisplay::FULL_REFRESH : EInkDisplay::FAST_REFRESH, false);
    if (first) {
      while (rendererDisplay->refreshBusy()) vTaskDelay(pdMS_TO_TICKS(20));
      rendererDisplay->skipInitialResync();
    }
    lastRefresh = millis(); first = false;
    vTaskDelay(1);
  }
}

void stopRenderer() {
  if (rendererTaskHandle) { vTaskDelete(rendererTaskHandle); rendererTaskHandle = nullptr; }
  if (rendererQueue) { vQueueDelete(rendererQueue); rendererQueue = nullptr; }
  if (rendererDisplay) { rendererDisplay->releaseBuffers(); if (rendererBegun) rendererDisplay->deepSleep(); delete rendererDisplay; rendererDisplay = nullptr; }
  rendererBegun = false;
  rendererEnabled = false;
}

bool startRenderer() {
  if (BoardConfig::ACTIVE.displayController != BoardConfig::DisplayController::UC8253) {
    Serial.println("Display rendering disabled: controller is not UC8253; serial emulator remains active");
    return false;
  }
  const auto& p = BoardConfig::ACTIVE.display;
  rendererDisplay = new (std::nothrow) EInkDisplay(p.sclk, p.mosi, p.cs, p.dc, p.rst, p.busy);
  if (!rendererDisplay) { Serial.println("Display renderer: EInkDisplay allocation failed"); return false; }
  rendererDisplay->setDisplayX3();
  if (!initializeX3SharedSpi()) { delete rendererDisplay; rendererDisplay = nullptr; return false; }
  rendererDisplay->begin();
  rendererBegun = true;
  if (!rendererDisplay->framebufferReady()) { Serial.println("Display renderer: framebuffer allocation failed"); stopRenderer(); return false; }
  rendererQueue = xQueueCreate(1, sizeof(tamaink::tamalib::Snapshot));
  if (!rendererQueue) { Serial.println("Display renderer: queue allocation failed"); stopRenderer(); return false; }
  if (xTaskCreate(rendererTask, "tama-render", 4096, nullptr, 1, &rendererTaskHandle) != pdPASS) {
    Serial.println("Display renderer: task allocation failed"); stopRenderer(); return false;
  }
  rendererEnabled = true;
  Serial.println("Display renderer: UC8253 X3 active (16x scale, centered CCW portrait; P1 order confirmed; bottom-row layout validation pending)");
  return true;
}

struct RomFileSource { FsFile* file; };
bool romSize(void* context, size_t* size) {
  if (!context || !size) return false;
  auto* source = static_cast<RomFileSource*>(context);
  *size = static_cast<size_t>(source->file->size());
  return true;
}
const char* romStatusName(tamaink::rom::Status status) {
  switch (status) {
    case tamaink::rom::Status::MissingSource: return "missing source";
    case tamaink::rom::Status::SizeFailure: return "size read failure";
    case tamaink::rom::Status::SizeMismatch: return "wrong size";
    case tamaink::rom::Status::ReadError: return "read failure";
    case tamaink::rom::Status::AllocationFailure: return "allocation failure";
    case tamaink::rom::Status::InvalidEncoding: return "invalid encoding";
    default: return "invalid ROM";
  }
}
size_t romRead(void* context, size_t offset, uint8_t* destination, size_t capacity) {
  if (!context || !destination) return 0;
  auto* source = static_cast<RomFileSource*>(context);
  if (!source->file->seek(offset)) return 0;
  const int got = source->file->read(destination, capacity);
  return got < 0 ? 0 : static_cast<size_t>(got);
}
void printEmulatorSnapshot(const tamaink::tamalib::Snapshot& s) {
  Serial.println("EMU LCD:");
  for (unsigned row = 0; row < 16; ++row) {
    for (unsigned col = 0; col < 32; ++col) Serial.print((s.lcd[row] & (1u << col)) ? '#' : '.');
    Serial.println();
  }
  Serial.printf("EMU ICONS: 0x%02X\n", s.icons);
}
void updateEmulatorInput() {
  inputManager.update();
  const uint8_t physical[3] = {InputManager::BTN_BACK, InputManager::BTN_CONFIRM, InputManager::BTN_POWER};
  const char labels[3] = {'A', 'B', 'C'};
  for (unsigned i = 0; i < 3; ++i) {
    if (inputManager.wasPressed(physical[i])) {
      const auto status = emulator.set_button(static_cast<tamaink::tamalib::Button>(i), true);
      Serial.printf("EMU INPUT: %c pressed%s\n", labels[i],
                    status == tamaink::tamalib::Status::Ok ? "" : " (adapter error)");
    }
    if (inputManager.wasReleased(physical[i])) {
      const auto status = emulator.set_button(static_cast<tamaink::tamalib::Button>(i), false);
      Serial.printf("EMU INPUT: %c released%s\n", labels[i],
                    status == tamaink::tamalib::Status::Ok ? "" : " (adapter error)");
    }
  }
}
bool startEmulator() {
  FsFile file = SdMan.open("/rom.bin", O_RDONLY);
  if (!file) { Serial.println("Emulator: ROM missing (/rom.bin)"); return false; }
  RomFileSource source{&file};
  tamaink::rom::ReadOnlySource reader{&source, romSize, romRead};
  struct Alloc { static uint16_t* alloc(void*, size_t n) { return new (std::nothrow) uint16_t[n]; }
    static void free(void*, uint16_t* p) { delete[] p; } };
  tamaink::rom::Allocator allocator{nullptr, Alloc::alloc, Alloc::free};
  tamaink::rom::Validation validation{};
  tamaink::rom::Status status = tamaink::rom::load(&reader, &allocator, &emulatorProgram, &validation);
  file.close();
  if (status != tamaink::rom::Status::Ok) { Serial.printf("Emulator: ROM validation failed: %s\n", romStatusName(status)); return false; }
  if (validation.classification != tamaink::rom::Classification::SupportedP1) {
    Serial.println("Emulator: ROM unsupported P1 variant"); delete[] emulatorProgram; emulatorProgram = nullptr; return false;
  }
  // Stable identity is derived solely from validated ROM CRC; ROM bytes remain read-only.
  kPersistRom[0] = 'T'; kPersistRom[1] = 'I'; kPersistRom[2] = 'N'; kPersistRom[3] = 'K';
  for (unsigned i = 0; i < 4; ++i) kPersistRom[4 + i] = static_cast<uint8_t>(validation.crc32 >> (8 * i));
  persistenceIdentityReady = true;
  const auto init = emulator.init(emulatorProgram, tamaink::tamalib::kProgramWords, &emulatorSnapshot);
  if (init != tamaink::tamalib::Status::Ok) { Serial.printf("Emulator: init failed (%u)\n", static_cast<unsigned>(init)); delete[] emulatorProgram; emulatorProgram = nullptr; return false; }
  scanPersistence();
  bool resumed = false;
  if (persistenceHasSelected) {
    const uint8_t first = persistenceSlot;
    const uint8_t order[2] = {first, static_cast<uint8_t>(1 - first)};
    for (uint8_t oi = 0; oi < 2 && !resumed; ++oi) {
      const uint8_t slot = order[oi];
      if (!readPersistenceSlot(slot, persistenceBootRecord[slot])) { Serial.printf("Persistence: slot %c rejected on read\n", 'A' + slot); continue; }
      if (!payloadMatches(persistenceBootRecord[slot])) { Serial.printf("Persistence: slot %c rejected (nested state)\n", 'A' + slot); continue; }
      if (tamaink::emulator::decode(persistenceBootRecord[slot].payload, persistenceBootRecord[slot].payloadLength, kPersistRom, &persistenceBootState) != tamaink::emulator::DecodeError::None) continue;
      if (emulator.import_state(persistenceBootState) != tamaink::tamalib::Status::Ok || emulator.snapshot(&emulatorSnapshot) != tamaink::tamalib::Status::Ok) {
        Serial.printf("Persistence: slot %c rejected (import/snapshot)\n", 'A' + slot); continue;
      }
      persistenceSlot = slot; persistenceGeneration = persistenceBootRecord[slot].generation; resumed = true;
      Serial.printf("Persistence: resumed generation=%lu from slot %c\n", static_cast<unsigned long>(persistenceGeneration), 'A' + slot);
    }
  }
  if (!resumed) Serial.println("Persistence: no importable state; starting fresh");
  Serial.println("Commands: l toggle LCD frames; p begin-save; n next-phase; c corrupt-newest; x cleanup-owned-state");
  emulatorActive = true;
  Serial.println("Emulator: active; physical BACK=A, CONFIRM=B, POWER=C; display refresh bypassed");
  printEmulatorSnapshot(emulatorSnapshot); emulatorPrinted = emulatorSnapshot; emulatorPrintedValid = true;
  emulatorObserved = emulatorSnapshot; emulatorObservedValid = true;
  serialFramePending = false; rendererFramePending = false; emulatorLastPrintAt = millis();
  return true;
}
bool persistenceReady = false;
enum class PersistenceState : uint8_t { Idle, Open, Partial, Complete, Synced, Verified, AwaitReset };
PersistenceState persistenceState = PersistenceState::Idle;
bool persistenceHasSelected = false;
uint8_t persistenceSlot = 0;
uint32_t persistenceGeneration = 0;
uint8_t persistencePendingSlot = 0;
uint32_t persistencePendingGeneration = 0;
uint8_t persistenceRecord[ tamaink::persist::kHeaderSize + tamaink::persist::kMaxPayload ]{};
uint8_t persistenceExpected[ tamaink::persist::kHeaderSize + tamaink::persist::kMaxPayload ]{};
size_t persistenceRecordSize = 0;
FsFile persistenceFile;
constexpr char kPersistDir[] = "/.tamaink";
constexpr const char* kPersistPaths[] = {
    "/.tamaink/state-a.bin",
    "/.tamaink/state-b.bin",
};
uint8_t kPersistRom[8] = {};
bool persistenceIdentityReady = false;
tamaink::persist::Record persistenceBootRecord[2]{};
tamaink::emulator::State persistenceBootState{};
uint8_t persistenceCandidate[tamaink::persist::kHeaderSize + tamaink::persist::kMaxPayload]{};
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

bool readX3RtcRegister(uint8_t reg, uint8_t& value) {
  const auto& sensor = BoardConfig::ACTIVE.sensors;
  if (sensor.rtcAddr == 0 || sensor.i2cSda < 0 || sensor.i2cScl < 0 || sensor.i2cHz == 0) return false;
  Wire.begin(sensor.i2cSda, sensor.i2cScl, sensor.i2cHz);
  Wire.setTimeOut(6);
  Wire.beginTransmission(sensor.rtcAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(sensor.rtcAddr, static_cast<uint8_t>(1), static_cast<uint8_t>(true)) < 1) return false;
  value = Wire.read();
  return true;
}

bool readX3RtcTime(uint8_t raw[7]) {
  const auto& sensor = BoardConfig::ACTIVE.sensors;
  if (sensor.rtcAddr == 0 || sensor.i2cSda < 0 || sensor.i2cScl < 0 || sensor.i2cHz == 0) return false;
  Wire.begin(sensor.i2cSda, sensor.i2cScl, sensor.i2cHz);
  Wire.setTimeOut(6);
  Wire.beginTransmission(sensor.rtcAddr);
  Wire.write(static_cast<uint8_t>(0x00));
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(sensor.rtcAddr, static_cast<uint8_t>(7), static_cast<uint8_t>(true)) < 7) return false;
  for (uint8_t i = 0; i < 7; ++i) raw[i] = Wire.read();
  return true;
}

uint8_t x3Bcd(uint8_t value) { return static_cast<uint8_t>((value >> 4) * 10U + (value & 0x0FU)); }

bool validBcd(uint8_t value) { return (value & 0x0FU) <= 9U && ((value >> 4) & 0x0FU) <= 9U; }

uint8_t daysInMonth(uint16_t year, uint8_t month) {
  constexpr uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 0;
  if (month != 2) return days[month - 1];
  const bool leap = (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
  return leap ? 29 : 28;
}

void runX3RtcBatteryDiagnostic() {
  Serial.println("RTC diagnostic: read-only DS3231 check");
  if (!BoardConfig::hasRtc()) {
    Serial.println("RTC: unavailable");
  } else {
    uint8_t status = 0;
    if (!readX3RtcRegister(0x0F, status)) {
      Serial.println("RTC: unavailable/I2C failure");
    } else if ((status & 0x80U) != 0) {
      Serial.println("RTC: present but oscillator-stopped (OSF)");
    } else {
      uint8_t raw[7] = {};
      if (!readX3RtcTime(raw)) {
        Serial.println("RTC: unavailable/I2C failure");
      } else {
        const uint8_t secondRaw = raw[0] & 0x7FU;
        const uint8_t minuteRaw = raw[1] & 0x7FU;
        const bool twelveHour = (raw[2] & 0x40U) != 0;
        const uint8_t hourRaw = raw[2] & (twelveHour ? 0x1FU : 0x3FU);
        const uint8_t weekdayRaw = raw[3] & 0x07U;
        const uint8_t dayRaw = raw[4] & 0x3FU;
        const uint8_t monthRaw = raw[5] & 0x1FU;
        const uint8_t yearRaw = raw[6];
        const uint8_t second = x3Bcd(secondRaw);
        const uint8_t minute = x3Bcd(minuteRaw);
        uint8_t hour = x3Bcd(hourRaw);
        if (twelveHour) {
          if (hour == 12) hour = 0;
          if ((raw[2] & 0x20U) != 0) hour = static_cast<uint8_t>(hour + 12);
        }
        const uint8_t weekday = x3Bcd(weekdayRaw);
        const uint8_t day = x3Bcd(dayRaw);
        const uint8_t month = x3Bcd(monthRaw);
        const uint16_t century = (raw[5] & 0x80U) != 0 ? 2100U : 2000U;
        const uint16_t year = static_cast<uint16_t>(century + x3Bcd(yearRaw));
        const bool encodingValid = validBcd(secondRaw) && validBcd(minuteRaw) && validBcd(hourRaw) &&
                                   validBcd(dayRaw) && validBcd(monthRaw) && validBcd(yearRaw) &&
                                   (raw[0] & 0x80U) == 0 && (raw[1] & 0x80U) == 0 &&
                                   (raw[2] & 0x80U) == 0 &&
                                   (raw[3] & 0xF8U) == 0 && (raw[4] & 0xC0U) == 0 &&
                                   (raw[5] & 0x60U) == 0;
        const bool valuesValid = second <= 59 && minute <= 59 && hour <= 23 && weekday >= 1 && weekday <= 7 &&
                                 month >= 1 && month <= 12 && day >= 1 && day <= daysInMonth(year, month) &&
                                 (!twelveHour || (x3Bcd(hourRaw) >= 1 && x3Bcd(hourRaw) <= 12));
        if (!encodingValid || !valuesValid) {
          Serial.println("RTC: present but time/date registers invalid");
        } else {
          Serial.printf("RTC: valid %04u-%02u-%02uT%02u:%02u:%02u weekday=%u\n", year, month, day, hour, minute,
                        second, weekday);
        }
      }
    }
  }

  Serial.println("Battery diagnostic: read-only BQ27220 check");
  const BatteryMonitor::Status status = BatteryMonitor().readStatus();
  if (!status.supported) {
    Serial.println("Battery: unsupported/unavailable");
    return;
  }
  const bool anyKnown = status.percentageKnown || status.millivoltsKnown || status.chargingKnown;
  Serial.println(anyKnown ? "Battery: supported" : "Battery: unavailable");
  if (status.percentageKnown) Serial.printf("Battery percentage: %u%%\n", status.percentage);
  else Serial.println("Battery percentage: unknown");
  if (status.millivoltsKnown) Serial.printf("Battery millivolts: %u\n", status.millivolts);
  else Serial.println("Battery millivolts: unknown");
  if (status.chargingKnown) Serial.printf("Battery charging: %s\n", status.charging ? "yes" : "no");
  else Serial.println("Battery charging: unknown");
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
  if (!SdMan.ready() && !SdMan.begin()) {
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

bool readPersistenceSlot(uint8_t slot, tamaink::persist::Record& out) {
  FsFile f = SdMan.open(kPersistPaths[slot], O_RDONLY);
  if (!f) return false;
  const uint64_t size = f.size();
  if (size < tamaink::persist::kHeaderSize || size > sizeof persistenceRecord) { f.close(); return false; }
  size_t total = 0;
  while (total < static_cast<size_t>(size)) { const int n = f.read(persistenceRecord + total, static_cast<size_t>(size) - total); if (n <= 0) { f.close(); return false; } total += static_cast<size_t>(n); }
  f.close();
  return persistenceIdentityReady && tamaink::persist::decode(persistenceRecord, static_cast<size_t>(size), kPersistRom, out) == tamaink::persist::DecodeError::None;
}

bool payloadMatches(const tamaink::persist::Record& r) {
  if (r.payloadLength != tamaink::emulator::kEncodedSize) return false;
  return tamaink::emulator::decode(r.payload, r.payloadLength, kPersistRom, &persistenceBootState) == tamaink::emulator::DecodeError::None;
}

bool verifyStagedSlot(uint8_t slot) {
  FsFile f = SdMan.open(kPersistPaths[slot], O_RDONLY); if (!f) return false;
  const uint64_t size = f.size(); if (size != persistenceRecordSize) { f.close(); return false; }
  size_t total = 0; while (total < static_cast<size_t>(size)) { const int n = f.read(persistenceRecord + total, static_cast<size_t>(size) - total); if (n <= 0) { f.close(); return false; } total += static_cast<size_t>(n); } f.close();
  if (!tamaink::persist::verifyStagedRecord(persistenceRecord, static_cast<size_t>(size),
                                             persistenceExpected, persistenceRecordSize)) return false;
  std::memcpy(persistenceCandidate, persistenceRecord, size);
  if (!tamaink::persist::commitStagedRecord(persistenceCandidate, static_cast<size_t>(size),
                                             persistenceExpected, persistenceRecordSize)) return false;
  if (tamaink::persist::decode(persistenceCandidate, static_cast<size_t>(size), kPersistRom, persistenceBootRecord[0]) != tamaink::persist::DecodeError::None) return false;
  return payloadMatches(persistenceBootRecord[0]);
}

void scanPersistence() {
  if (!SdMan.ready()) { persistenceReady = false; Serial.println("Persistence: SD unavailable; commands refused"); return; }
  persistenceReady = true; bool valid[2]{};
  for (uint8_t i = 0; i < 2; ++i) { valid[i] = readPersistenceSlot(i, persistenceBootRecord[i]) && payloadMatches(persistenceBootRecord[i]); Serial.printf("Persistence slot %c: %s\n", 'A' + i, valid[i] ? "valid" : "invalid/missing"); }
  const int newest = tamaink::persist::selectNewest(&persistenceBootRecord[0], valid[0], &persistenceBootRecord[1], valid[1]);
  persistenceHasSelected = newest >= 0;
  if (newest < 0) { persistenceSlot = 0; persistenceGeneration = 0; }
  if (newest >= 0) { persistenceSlot = static_cast<uint8_t>(newest); persistenceGeneration = persistenceBootRecord[newest].generation; Serial.printf("Persistence selected slot %c generation=%lu\n", 'A' + newest, static_cast<unsigned long>(persistenceGeneration)); }
}

void failPersistenceWrite(const char* why) { if (persistenceFile) persistenceFile.close(); persistenceState = PersistenceState::Idle; Serial.printf("Persistence transaction failed: %s; rescanning\n", why); scanPersistence(); }

void persistenceCommand(char c) {
  if (c != 'p' && c != 'n' && c != 'c' && c != 'x') return;
  if (!emulatorActive) { Serial.println("Persistence command refused: emulator inactive"); return; }
  if (!persistenceReady) { Serial.println("Persistence command refused: SD unavailable"); return; }
  if (persistenceState == PersistenceState::AwaitReset) { Serial.println("Persistence command refused: await physical reset"); return; }
  if (c == 'p') {
    if (persistenceState != PersistenceState::Idle) return;
    if (!SdMan.ensureDirectoryExists(kPersistDir)) { Serial.println("Persistence p refused: directory create failed"); return; }
    const uint8_t target = persistenceHasSelected ? static_cast<uint8_t>(1 - persistenceSlot) : 0;
    const uint32_t next = persistenceHasSelected ? persistenceGeneration + 1u : 0u;
    tamaink::persist::Record& r = persistenceBootRecord[0]; r = {}; std::memcpy(r.rom, kPersistRom, 8); r.generation = next; r.timestamp = millis();
    if (emulator.export_state(&persistenceBootState) != tamaink::tamalib::Status::Ok) { Serial.println("Persistence p refused: live export failed"); return; }
    r.payloadLength = tamaink::emulator::encode(persistenceBootState, r.payload, sizeof r.payload, kPersistRom);
    if (r.payloadLength != tamaink::emulator::kEncodedSize) { Serial.println("Persistence p refused: state encode failed"); return; }
    persistenceRecordSize = tamaink::persist::encode(r, persistenceRecord, sizeof persistenceRecord);
    if (persistenceRecordSize == 0) { Serial.println("Persistence p refused: encode failed"); return; }
    std::memcpy(persistenceExpected, persistenceRecord, persistenceRecordSize);
    if (!tamaink::persist::stageEncodedRecord(persistenceRecord, persistenceRecordSize)) { Serial.println("Persistence p refused: staging failed"); return; }
    persistencePendingSlot = target; persistencePendingGeneration = next;
    persistenceFile = SdMan.open(kPersistPaths[target], O_RDWR | O_CREAT | O_TRUNC);
    if (!persistenceFile) { failPersistenceWrite("open"); return; }
    persistenceState = PersistenceState::Open;
    Serial.printf("Persistence p: target slot %c generation=%lu; reset now or send n for Partial\n", 'A' + target, static_cast<unsigned long>(next)); return;
  }
  if (c == 'n') {
    if (persistenceState == PersistenceState::Idle) return;
    const size_t partial = tamaink::persist::kHeaderSize / 2;
    if (persistenceState == PersistenceState::Open) { if (persistenceFile.write(persistenceRecord, partial) != partial) { failPersistenceWrite("partial write"); return; } persistenceState = PersistenceState::Partial; Serial.println("Persistence Partial complete; reset now or send n"); }
    else if (persistenceState == PersistenceState::Partial) { const size_t rem = persistenceRecordSize - partial; if (persistenceFile.write(persistenceRecord + partial, rem) != rem) { failPersistenceWrite("remainder write"); return; } persistenceState = PersistenceState::Complete; Serial.println("Persistence CompleteUnsynced; reset now or send n"); }
    else if (persistenceState == PersistenceState::Complete) { if (!persistenceFile.sync()) { failPersistenceWrite("sync"); return; } persistenceState = PersistenceState::Synced; Serial.println("Persistence Synced; reset now or send n"); }
    else if (persistenceState == PersistenceState::Synced) { persistenceFile.close(); if (!verifyStagedSlot(persistencePendingSlot)) { failPersistenceWrite("staged validation"); return; } persistenceState = PersistenceState::Verified; Serial.println("Persistence Verified staged; reset now or send n to commit"); }
    else if (persistenceState == PersistenceState::Verified) { persistenceFile = SdMan.open(kPersistPaths[persistencePendingSlot], O_RDWR); if (!persistenceFile || !persistenceFile.seek(tamaink::persist::kCrcOffset) || persistenceFile.write(persistenceExpected + tamaink::persist::kCrcOffset, tamaink::persist::kCrcSize) != tamaink::persist::kCrcSize || !persistenceFile.sync()) { failPersistenceWrite("commit"); return; } persistenceFile.close(); if (!readPersistenceSlot(persistencePendingSlot, persistenceBootRecord[0]) || !payloadMatches(persistenceBootRecord[0]) || persistenceBootRecord[0].generation != persistencePendingGeneration) { Serial.println("Persistence commit rejected: reread validation failed; prior slot retained"); persistenceState = PersistenceState::Idle; scanPersistence(); return; } persistenceSlot = persistencePendingSlot; persistenceGeneration = persistencePendingGeneration; persistenceHasSelected = true; persistenceState = PersistenceState::Idle; Serial.println("Persistence commit complete and verified"); }
    return;
  }
  if (persistenceState != PersistenceState::Idle) return;
  if (c == 'c') {
    if (!persistenceHasSelected || !readPersistenceSlot(persistenceSlot, persistenceBootRecord[0]) || !payloadMatches(persistenceBootRecord[0]) || !readPersistenceSlot(static_cast<uint8_t>(1 - persistenceSlot), persistenceBootRecord[1]) || !payloadMatches(persistenceBootRecord[1]) || !tamaink::persist::generationNewer(persistenceBootRecord[0].generation, persistenceBootRecord[1].generation)) { Serial.println("Persistence c refused: need newest plus older valid slots"); return; }
    FsFile f = SdMan.open(kPersistPaths[persistenceSlot], O_RDWR);
    uint8_t b = 0;
    const bool ioOk = f && f.seek(tamaink::persist::kHeaderSize) && f.read(&b, 1) == 1 &&
                      f.seek(tamaink::persist::kHeaderSize);
    if (!ioOk) { if (f) f.close(); Serial.println("Persistence c failed"); return; }
    b ^= 0x01u;
    if (f.write(&b, 1) != 1 || !f.sync()) { f.close(); Serial.println("Persistence c failed"); return; }
    f.close(); persistenceState = PersistenceState::AwaitReset; Serial.println("Persistence newest corrupted; await physical reset"); return;
  }
  if (c == 'x') { const bool a = !SdMan.exists(kPersistPaths[0]) || SdMan.remove(kPersistPaths[0]); const bool b = !SdMan.exists(kPersistPaths[1]) || SdMan.remove(kPersistPaths[1]); if (!a || !b) { Serial.println("Persistence cleanup failed"); scanPersistence(); return; } if (SdMan.exists(kPersistDir)) SdMan.rmdir(kPersistDir); persistenceHasSelected = false; persistenceGeneration = 0; persistenceSlot = 0; Serial.println("Persistence owned files cleaned"); }
}

void dispatchSerialCommand(char c) {
  if (c == 'l') {
    serialLcdFramesEnabled = !serialLcdFramesEnabled;
    if (serialLcdFramesEnabled) {
      serialFramePending = emulatorActive;
      emulatorLastPrintAt = millis() - EMULATOR_SERIAL_FRAME_INTERVAL_MS;
    } else {
      serialFramePending = false;
    }
    Serial.printf("Serial LCD frames: %s\n", serialLcdFramesEnabled ? "enabled" : "disabled");
    return;
  }
  persistenceCommand(c);
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
    // XteinkDetect has completed and released its temporary I2C bus use here.
    runX3RtcBatteryDiagnostic();
    freeink::applyXteinkDisplayController();
    Serial.printf("Display controller: %s\n", displayControllerName(BoardConfig::ACTIVE.displayController));

    if (!SdMan.begin()) {
      Serial.println("Emulator: SD mount failed; continuing hardware diagnostics");
    } else if (startEmulator()) {
      beginInputDiagnostic();
      if (startRenderer()) xQueueOverwrite(rendererQueue, &emulatorSnapshot);
      return;
    }

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
    scanPersistence();
    Serial.println("Persistence gate: p=begin live save, n=advance phase (repeat), c=corrupt newest, x=cleanup owned state paths");
    beginInputDiagnostic();
  } else {
    Serial.println("Board detection stopped; display pins untouched.");
  }
}

void loop() {
  if (emulatorActive) {
    updateEmulatorInput();
    while (Serial.available()) dispatchSerialCommand(static_cast<char>(Serial.read()));
    emulator.step(64, &emulatorSnapshot);
    const bool lcdChanged = !emulatorObservedValid ||
        std::memcmp(emulatorSnapshot.lcd, emulatorObserved.lcd, sizeof emulatorSnapshot.lcd) != 0;
    const bool iconChanged = !emulatorObservedValid || emulatorSnapshot.icons != emulatorObserved.icons;
    if (lcdChanged || iconChanged) {
      emulatorObserved = emulatorSnapshot;
      emulatorObservedValid = true;
      serialFramePending = serialLcdFramesEnabled;
      if (lcdChanged || iconChanged) rendererFramePending = true;
    }
    const unsigned long now = millis();
    if (serialLcdFramesEnabled && serialFramePending && now - emulatorLastPrintAt >= EMULATOR_SERIAL_FRAME_INTERVAL_MS) {
      printEmulatorSnapshot(emulatorSnapshot);
      emulatorPrinted = emulatorSnapshot;
      emulatorPrintedValid = true;
      serialFramePending = false;
      emulatorLastPrintAt = now;
    }
    if (rendererEnabled && rendererFramePending && xQueueOverwrite(rendererQueue, &emulatorSnapshot) == pdPASS)
      rendererFramePending = false;
    delay(0);
    return;
  }
  if (inputReady) updateInputDiagnostic();
  while (Serial.available()) dispatchSerialCommand(static_cast<char>(Serial.read()));
  delay(10);
}
