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
#include <esp_sleep.h>
#include <cstring>
#include <new>
#include "tamaink_persistence.h"
#include "tamaink_rom.h"
#include "tamaink_tamalib.h"
#include "tamaink_renderer.h"
#include "tamaink_renderer_refresh.h"
#include "tamaink_renderer_dispatch.h"
#include "tamaink_emulator_state.h"
#include "tamaink_autosave.h"
#include "tamaink_wake_diagnostic.h"
#include "tamaink_sleep_gesture.h"
#include "tamaink_sleep_screen.h"
#include "tamaink_rtc_sleep_gate.h"
#include "tamaink_wake_catchup_plan.h"
#include "tamaink_wake_catchup.h"
#include "tamaink_battery_telemetry.h"
#include "tamaink_low_battery_sleep.h"
#include "tamaink_settings.h"
#include <Preferences.h>
#include <PowerManager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#ifndef TAMAINK_VERSION
#define TAMAINK_VERSION "unknown"
#endif

namespace {

constexpr uint8_t BUTTON_COUNT = InputManager::BTN_POWER + 1;
constexpr unsigned long INPUT_REPOLL_MS = 6;
constexpr unsigned long HOLD_REPORT_INTERVAL_MS = 1000;
constexpr unsigned long RENDERER_MIN_REFRESH_INTERVAL_MS = 400;
constexpr unsigned long EMULATOR_SERIAL_FRAME_INTERVAL_MS = 250;

InputManager inputManager;
bool inputReady = false;
bool emulatorActive = false;
tamaink::battery::WarningPolicy batteryWarning;
tamaink::battery::Schedule batteryTelemetrySchedule;
volatile bool batteryPercentageKnown = false;
volatile std::uint8_t batteryPercentage = 0;
tamaink::low_battery::Coordinator lowBatterySleep;
tamaink::low_battery::TransactionArbiter saveArbiter;
tamaink::tamalib::Adapter emulator;
std::uint16_t* emulatorProgram = nullptr;
tamaink::tamalib::Snapshot emulatorSnapshot{};
tamaink::tamalib::Snapshot emulatorPrinted{};
bool emulatorPrintedValid = false;
tamaink::tamalib::Snapshot emulatorObserved{};
bool emulatorObservedValid = false;
bool serialFramePending = false;
bool serialLcdFramesEnabled = false;
tamaink::autosave::Controller autosaveController{};
tamaink::settings::Controller settingsController;
uint32_t autosaveInterval() {
  return tamaink::settings::autosaveIntervalMs(settingsController.values().autosave);
}
uint32_t lcdInterval() {
  return tamaink::settings::displayIntervalMs(settingsController.values().display);
}
constexpr uint32_t AUTOSAVE_RETRY_MS = 60UL * 1000UL;
tamaink::render::FrameDispatch rendererDispatch;
Preferences settingsPrefs;
unsigned long emulatorLastPrintAt = 0;
EInkDisplay* rendererDisplay = nullptr;
QueueHandle_t rendererQueue = nullptr;
struct RenderPacket {
  tamaink::tamalib::Snapshot frame{};
  tamaink::settings::Values settings{};
  bool menu = false;
  std::uint8_t focus = 0;
  bool batteryKnown = false;
  std::uint8_t battery = 0;
  bool forceFull = false;
};
bool rendererForceFull = false;
RenderPacket makeRenderPacket() {
  return {emulatorSnapshot, settingsController.values(), settingsController.open(),
          settingsController.focus(), batteryPercentageKnown, batteryPercentage,
          rendererForceFull};
}
BaseType_t enqueueRenderer() {
  RenderPacket packet = makeRenderPacket();
  RenderPacket queued{};
  if (xQueuePeek(rendererQueue, &queued, 0) == pdTRUE)
    packet.forceFull = tamaink::render::preserveFullRefresh(queued.forceFull,
                                                            packet.forceFull);
  const BaseType_t result = xQueueOverwrite(rendererQueue, &packet);
  if (result == pdPASS) rendererForceFull = false;
  return result;
}
TaskHandle_t rendererTaskHandle = nullptr;
SemaphoreHandle_t rendererStopped = nullptr;
SemaphoreHandle_t rendererFirstFrameDone = nullptr;
volatile bool rendererFirstFrameConfirmed = false;
bool rendererEnabled = false;
bool rendererBegun = false;
volatile bool rendererStopRequested = false;
tamaink::wake::Coordinator wakeDiagnostic;
tamaink::sleep_gesture::Controller sleepGesture;
bool wakeCatchupPending = false;
std::uint64_t wakeCatchupElapsedSeconds = 0;
std::uint32_t wakeCatchupVirtualTimestamp = 0;
std::uint32_t wakeCatchupTimestampFrequency = 0;
void dispatchSerialCommand(char c);
bool initializeX3SharedSpi();
bool readRtcEpoch(std::uint64_t& epoch);
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
  RenderPacket packet{};
  bool first = true;
  tamaink::render::RefreshCadence cadence;
  unsigned long lastRefresh = 0;
  for (;;) {
    if (xQueueReceive(rendererQueue, &packet, pdMS_TO_TICKS(20)) != pdTRUE) {
      if (rendererStopRequested) break;
      continue;
    }
    const unsigned long busyStarted = millis();
    while (rendererDisplay->refreshBusy() && millis() - busyStarted < 12000UL) vTaskDelay(pdMS_TO_TICKS(20));
    if (rendererDisplay->refreshBusy()) {
      Serial.println("Display renderer: BUSY timeout before frame");
      if (first) { rendererFirstFrameConfirmed = false; if (rendererFirstFrameDone) xSemaphoreGive(rendererFirstFrameDone); }
      continue;
    }
    const unsigned long now = millis();
    if (!first && now - lastRefresh < RENDERER_MIN_REFRESH_INTERVAL_MS)
      vTaskDelay(pdMS_TO_TICKS(RENDERER_MIN_REFRESH_INTERVAL_MS - (now - lastRefresh)));
    RenderPacket newest{};
    while (xQueueReceive(rendererQueue, &newest, 0) == pdTRUE) {
      newest.forceFull = tamaink::render::preserveFullRefresh(packet.forceFull,
                                                              newest.forceFull);
      packet = newest;
    }
    const std::uint8_t threshold = tamaink::settings::cleaningThreshold(packet.settings.display);
    cadence.setThreshold(threshold);
    const auto status = tamaink::render::snapshot(packet.frame, rendererDisplay->getFrameBuffer(),
        rendererDisplay->getBufferSize(), rendererDisplay->getDisplayWidth(), rendererDisplay->getDisplayHeight(),
        rendererDisplay->getDisplayWidthBytes(), 268, 8, 16, tamaink::render::Rotation::CounterClockwise90,
        tamaink::render::IconLayout::P1BottomRow,
        tamaink::render::BatteryStatus{packet.batteryKnown, packet.battery,
          packet.settings.battery == tamaink::settings::Battery::Show});
    if (status != tamaink::render::Status::Ok) {
      Serial.println("Display renderer: frame geometry rejected");
      if (first) { rendererFirstFrameConfirmed = false; if (rendererFirstFrameDone) xSemaphoreGive(rendererFirstFrameDone); }
      continue;
    }
    if (packet.menu) tamaink::render::overlaySettings(rendererDisplay->getFrameBuffer(), rendererDisplay->getBufferSize(), rendererDisplay->getDisplayWidth(), rendererDisplay->getDisplayHeight(), rendererDisplay->getDisplayWidthBytes(), packet.settings, packet.focus);
    const auto kind = packet.forceFull ? tamaink::render::RefreshKind::Full : cadence.next();
    const bool periodicPromotion = !first && kind == tamaink::render::RefreshKind::Full && !packet.forceFull;
    rendererDisplay->displayBuffer(kind == tamaink::render::RefreshKind::Full ? EInkDisplay::FULL_REFRESH
                                                                                : EInkDisplay::FAST_REFRESH,
                                    false);
    cadence.presented(kind);
    if (first) {
      const unsigned long refreshStarted = millis();
      while (rendererDisplay->refreshBusy() && millis() - refreshStarted < 12000UL) vTaskDelay(pdMS_TO_TICKS(20));
      rendererFirstFrameConfirmed = !rendererDisplay->refreshBusy();
      if (rendererFirstFrameConfirmed) rendererDisplay->skipInitialResync();
      if (rendererFirstFrameDone) xSemaphoreGive(rendererFirstFrameDone);
    } else if (periodicPromotion) {
      Serial.printf("Display renderer: periodic full refresh after %u fast frames\n", threshold);
    }
    lastRefresh = millis(); first = false;
    if (rendererStopRequested) break;
    vTaskDelay(1);
  }
  if (rendererStopped) xSemaphoreGive(rendererStopped);
  vTaskSuspend(nullptr);
}

void stopRenderer() {
  rendererStopRequested = true;
  if (rendererTaskHandle && rendererStopped) {
    if (xSemaphoreTake(rendererStopped, portMAX_DELAY) == pdTRUE) {
      vTaskDelete(rendererTaskHandle);
      rendererTaskHandle = nullptr;
    }
  }
  if (rendererStopped) { vSemaphoreDelete(rendererStopped); rendererStopped = nullptr; }
  if (rendererFirstFrameDone) { vSemaphoreDelete(rendererFirstFrameDone); rendererFirstFrameDone = nullptr; }
  rendererFirstFrameConfirmed = false;
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
  rendererStopRequested = false;
  if (!initializeX3SharedSpi()) { delete rendererDisplay; rendererDisplay = nullptr; return false; }
  rendererDisplay->begin();
  rendererBegun = true;
  if (!rendererDisplay->framebufferReady()) { Serial.println("Display renderer: framebuffer allocation failed"); stopRenderer(); return false; }
  rendererQueue = xQueueCreate(1, sizeof(RenderPacket));
  if (!rendererQueue) { Serial.println("Display renderer: queue allocation failed"); stopRenderer(); return false; }
  rendererStopped = xSemaphoreCreateBinary();
  if (!rendererStopped) { Serial.println("Display renderer: completion semaphore allocation failed"); stopRenderer(); return false; }
  rendererFirstFrameDone = xSemaphoreCreateBinary();
  if (!rendererFirstFrameDone) { Serial.println("Display renderer: first-frame semaphore allocation failed"); stopRenderer(); return false; }
  rendererFirstFrameConfirmed = false;
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
  const bool menuWasOpen = settingsController.open();
  const auto before = settingsController.values();
  const auto menuEvent = settingsController.update(
      static_cast<std::uint32_t>(millis()),
      inputManager.isPressed(InputManager::BTN_UP),
      inputManager.wasPressed(InputManager::BTN_BACK),
      inputManager.wasPressed(InputManager::BTN_CONFIRM),
      inputManager.wasPressed(InputManager::BTN_POWER));
  if (menuEvent != tamaink::settings::Controller::Event::None) {
    rendererForceFull = menuWasOpen != settingsController.open();
    rendererDispatch.setInterval(lcdInterval());
    const auto& after = settingsController.values();
    if (before.autosave != after.autosave)
      autosaveController.arm(millis(), autosaveInterval());
    const bool valuesChanged = before.battery != after.battery ||
                               before.display != after.display ||
                               before.autosave != after.autosave;
    if (valuesChanged) {
      std::uint8_t encoded[5]{};
      if (tamaink::settings::encode(after, encoded, sizeof encoded) == sizeof encoded)
        settingsPrefs.putBytes("values", encoded, sizeof encoded);
    }
    if (rendererEnabled && rendererQueue) {
      enqueueRenderer();
      rendererDispatch.queued(millis());
    }
  }
  if (menuWasOpen || menuEvent != tamaink::settings::Controller::Event::None ||
      settingsController.open())
    return;
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
  const auto gestureEvent = sleepGesture.update(
      static_cast<uint32_t>(millis()), inputManager.isPressed(InputManager::BTN_BACK),
      inputManager.isPressed(InputManager::BTN_POWER));
  if (gestureEvent == tamaink::sleep_gesture::Controller::Event::Cancelled) {
    Serial.println("Wake diagnostic gesture canceled: BACK+POWER chord released before 2000 ms");
  } else if (gestureEvent == tamaink::sleep_gesture::Controller::Event::Trigger) {
    Serial.println("Wake diagnostic gesture released: requesting durable save");
    dispatchSerialCommand('w');
  }
}

void renderSleepScreenAndRelease() {
  if (!rendererDisplay || !rendererBegun || !rendererDisplay->framebufferReady()) {
    Serial.println("Sleep screen unavailable; continuing to ESP sleep");
    stopRenderer();
    return;
  }
  const auto status = tamaink::sleep_screen::render(rendererDisplay->getFrameBuffer(),
      rendererDisplay->getBufferSize(), rendererDisplay->getDisplayWidth(), rendererDisplay->getDisplayHeight(),
      rendererDisplay->getDisplayWidthBytes());
  if (status == tamaink::sleep_screen::Status::Ok) {
    rendererDisplay->displayBuffer(EInkDisplay::FULL_REFRESH, true);
  } else Serial.println("Sleep screen geometry rejected; continuing to ESP sleep");
  rendererDisplay->releaseBuffers();
  rendererDisplay->deepSleep();
  delete rendererDisplay; rendererDisplay = nullptr; rendererBegun = false; rendererEnabled = false;
  if (rendererStopped) { vSemaphoreDelete(rendererStopped); rendererStopped = nullptr; }
  if (rendererFirstFrameDone) { vSemaphoreDelete(rendererFirstFrameDone); rendererFirstFrameDone = nullptr; }
  rendererFirstFrameConfirmed = false;
  if (rendererQueue) { vQueueDelete(rendererQueue); rendererQueue = nullptr; }
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
      if (esp_reset_reason() == ESP_RST_DEEPSLEEP) {
        std::uint64_t saved = 0, nowRtc = 0;
        const bool sv = tamaink::rtc::decodeTagged(persistenceBootRecord[slot].timestamp, saved);
        const bool cv = readRtcEpoch(nowRtc);
        std::uint64_t elapsedSeconds = 0;
        if (sv && cv && tamaink::rtc::elapsed(saved, nowRtc, elapsedSeconds)) {
          const auto plan = tamaink::wake::makeCatchupPlan(
              elapsedSeconds, persistenceBootState.virtual_timestamp,
              persistenceBootState.tamalib_timestamp_frequency);
          if (plan.available) {
            // Defer catch-up until the imported snapshot has completed its first
            // e-ink refresh. This preserves the exact persisted frame on wake.
            wakeCatchupPending = true;
            wakeCatchupElapsedSeconds = elapsedSeconds;
            wakeCatchupVirtualTimestamp = persistenceBootState.virtual_timestamp;
            wakeCatchupTimestampFrequency = persistenceBootState.tamalib_timestamp_frequency;
          } else {
            Serial.printf("Wake catch-up plan unavailable: requested=%llu s; emulator catch-up not applied\n",
                          static_cast<unsigned long long>(elapsedSeconds));
          }
        }
        else if (sv && cv) Serial.println("Wake diagnostic: RTC moved backward; elapsed unavailable; emulator catch-up not applied");
        else Serial.println("Wake diagnostic: RTC saved/current timestamp unavailable or invalid; emulator catch-up not applied");
      }
    }
  }
  if (!resumed) Serial.println("Persistence: no importable state; starting fresh");
  Serial.println("Commands: a autosave-now; b battery telemetry; B inject one unplugged-low sample (dev); l toggle LCD frames; p manual-save; n next-phase; w save+deep-sleep wake diagnostic; c corrupt-newest; x cleanup-owned-state");
  rendererDispatch.setInterval(lcdInterval());
  autosaveController.arm(millis(), autosaveInterval());
  emulatorActive = true;
  batteryTelemetrySchedule.arm(millis());
  Serial.println("Emulator: active; physical BACK=A, CONFIRM=B, POWER=C; hold BACK+POWER >=2000 ms, then release for wake diagnostic; display refresh bypassed");
  printEmulatorSnapshot(emulatorSnapshot); emulatorPrinted = emulatorSnapshot; emulatorPrintedValid = true;
  emulatorObserved = emulatorSnapshot; emulatorObservedValid = true;
  serialFramePending = false; rendererDispatch.reset(); emulatorLastPrintAt = millis();
  return true;
}

void runDeferredWakeCatchup() {
  if (!wakeCatchupPending) return;
  wakeCatchupPending = false;
  const auto plan = tamaink::wake::makeCatchupPlan(
      wakeCatchupElapsedSeconds, wakeCatchupVirtualTimestamp, wakeCatchupTimestampFrequency);
  if (!plan.available) return;
  tamaink::wake::CatchupController controller(plan.targetVirtualTimestamp,
                                              plan.maxInstructionAttempts, emulatorSnapshot.timestamp);
  const unsigned long startedAt = millis();
  const auto fastForwardStatus = emulator.set_fast_forward(true);
  if (fastForwardStatus != tamaink::tamalib::Status::Ok)
    controller.stop(tamaink::wake::CatchupOutcome::AdapterError, emulatorSnapshot.timestamp);
  while (!controller.done()) {
    if (millis() - startedAt >= 10000UL) { controller.stop(tamaink::wake::CatchupOutcome::Watchdog, emulatorSnapshot.timestamp); break; }
    const std::size_t batch = controller.nextBatch(emulatorSnapshot.timestamp);
    if (!batch) break;
    tamaink::tamalib::Snapshot next{};
    const auto status = emulator.step(batch, &next);
    controller.observe(status == tamaink::tamalib::Status::Ok ? next.timestamp : emulatorSnapshot.timestamp,
                       batch, status == tamaink::tamalib::Status::Ok);
    if (status == tamaink::tamalib::Status::Ok) emulatorSnapshot = next;
  }
  const auto restoreStatus = emulator.set_fast_forward(false);
  if (restoreStatus != tamaink::tamalib::Status::Ok)
    Serial.printf("Wake catch-up: failed to restore normal clock mode (%u)\n", static_cast<unsigned>(restoreStatus));
  const auto result = controller.result();
  Serial.printf("Wake catch-up: requested=%llu s planned=%lu s outcome=%s attempts=%lu finalTicks=%lu targetTicks=%lu capped=%s\n",
                static_cast<unsigned long long>(plan.requestedSeconds), static_cast<unsigned long>(plan.plannedSeconds),
                tamaink::wake::catchupOutcomeName(result.outcome), static_cast<unsigned long>(result.attempts),
                static_cast<unsigned long>(result.finalTimestamp), static_cast<unsigned long>(result.targetTimestamp),
                plan.capped ? "yes" : "no");
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

bool readRtcEpoch(std::uint64_t& epoch) { uint8_t raw[7]{}; tamaink::rtc::DateTime dt{}; return readX3RtcTime(raw) && tamaink::rtc::decodeDs3231(raw, dt, epoch); }

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
        tamaink::rtc::DateTime dt{}; std::uint64_t epoch = 0;
        if (!tamaink::rtc::decodeDs3231(raw, dt, epoch)) {
          Serial.println("RTC: present but time/date registers invalid");
        } else {
          Serial.printf("RTC: valid %04u-%02u-%02uT%02u:%02u:%02u weekday=%u\n", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second, dt.weekday);
        }
      }
    }
  }

  Serial.println("Battery diagnostic: read-only BQ27220 check");
  const BatteryMonitor::Status status = BatteryMonitor().readStatus();
  batteryPercentageKnown = status.percentageKnown && status.percentage <= 100;
  batteryPercentage = batteryPercentageKnown ? static_cast<std::uint8_t>(status.percentage) : 0;
  if (status.percentageKnown && status.percentage <= 100) batteryWarning.update(true, status.percentage);
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

const char* batteryWarningName(tamaink::battery::WarningState state) {
  switch (state) {
    case tamaink::battery::WarningState::Low: return "low";
    case tamaink::battery::WarningState::Normal: return "normal";
    default: return "unknown";
  }
}

bool sampleBatteryTelemetry(const char* source) {
  if (wakeDiagnostic.pending || persistenceState != PersistenceState::Idle) {
    if (source[0] == 'm') Serial.println("Battery telemetry: busy");
    return false;
  }
  const BatteryMonitor::Status status = BatteryMonitor().readStatus();
  const bool oldBatteryKnown = batteryPercentageKnown;
  const std::uint8_t oldBatteryPercentage = batteryPercentage;
  const auto result = batteryWarning.update(status.percentageKnown, status.percentage);
  batteryPercentageKnown = status.percentageKnown && status.percentage <= 100;
  batteryPercentage = batteryPercentageKnown ? static_cast<std::uint8_t>(status.percentage) : 0;
  if (rendererEnabled && (oldBatteryKnown != batteryPercentageKnown || oldBatteryPercentage != batteryPercentage))
    rendererDispatch.changed(false, millis());
  Serial.printf("Battery telemetry source=%s supported=%s percentage=%s", source,
                status.supported ? "yes" : "no", status.percentageKnown && status.percentage <= 100 ? "known" : "unknown");
  if (status.percentageKnown && status.percentage <= 100) Serial.printf("(%u%%)", status.percentage);
  Serial.printf(" millivolts=%s", status.millivoltsKnown ? "known" : "unknown");
  if (status.millivoltsKnown) Serial.printf("(%u)", status.millivolts);
  Serial.printf(" charging=%s warning=%s\n", status.chargingKnown ? (status.charging ? "yes" : "no") : "unknown", batteryWarningName(result.state));
  if (result.changed) Serial.printf("Battery warning transition: %s\n", batteryWarningName(result.state));
  tamaink::low_battery::Sample sample{status.percentageKnown && status.percentage <= 100,
                                      status.percentage, status.chargingKnown, status.charging};
  const bool ready = emulatorActive && persistenceReady && persistenceState == PersistenceState::Idle &&
                     !autosaveController.automatic && !autosaveController.scheduler.immediate && !wakeDiagnostic.pending && !wakeDiagnostic.requested &&
                     persistenceState != PersistenceState::AwaitReset;
  if (lowBatterySleep.observe(sample, ready)) {
    if (!saveArbiter.claim(tamaink::low_battery::Owner::LowBattery)) {
      lowBatterySleep.saveFailed();
      Serial.println("Low-battery sleep refused: save transaction already owned");
      return true;
    }
    tamaink::autosave::request(autosaveController.scheduler);
    Serial.println("Low-battery sleep requested: durable save will begin");
  }
  return true;
}

void injectLowBatterySample() {
  if (lowBatterySleep.pending() || lowBatterySleep.latched()) {
    Serial.println("Battery telemetry source=diagnostic-B injected percentage=15% charging=no; low-battery request already pending/latched (ignored)");
    return;
  }
  const unsigned before = lowBatterySleep.consecutive();
  Serial.printf("Battery telemetry source=diagnostic-B injected percentage=15%% charging=no (confirmation %u/2)\n",
                before >= 1 ? 2u : 1u);
  const bool ready = emulatorActive && persistenceReady && persistenceState == PersistenceState::Idle &&
                     !autosaveController.automatic && !autosaveController.scheduler.immediate && !wakeDiagnostic.pending && !wakeDiagnostic.requested &&
                     persistenceState != PersistenceState::AwaitReset;
  if (lowBatterySleep.observe({true, 15, true, false}, ready)) {
    if (!saveArbiter.claim(tamaink::low_battery::Owner::LowBattery)) {
      lowBatterySleep.saveFailed();
      Serial.println("Low-battery sleep refused: save transaction already owned");
      return;
    }
    tamaink::autosave::request(autosaveController.scheduler);
    Serial.println("Low-battery sleep requested: durable save will begin");
  } else if (lowBatterySleep.consecutive() < 2) {
    Serial.printf("Low-battery confirmation %u/2; staying awake\n", lowBatterySleep.consecutive());
  }
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

void failPersistenceWrite(const char* why) { if (persistenceFile) persistenceFile.close(); persistenceState = PersistenceState::Idle; wakeDiagnostic.saveFailed(); if (saveArbiter.owner() == tamaink::low_battery::Owner::LowBattery) lowBatterySleep.saveFailed(); saveArbiter.fail(saveArbiter.owner()); autosaveController.writeFailure(millis(), AUTOSAVE_RETRY_MS); if (wakeDiagnostic.requested) Serial.println("Wake diagnostic canceled: save failed"); wakeDiagnostic.requested = false; Serial.printf("Persistence transaction failed: %s; rescanning\n", why); scanPersistence(); }

void enterWakeDiagnosticSleep(tamaink::low_battery::Owner sleepOwner = tamaink::low_battery::Owner::WakeDiagnostic) {
  const bool lowBatterySleepRequested = sleepOwner == tamaink::low_battery::Owner::LowBattery;
  Serial.println(lowBatterySleepRequested ? "Low-battery sleep: save verified; preparing deep sleep" : "Wake diagnostic: save verified; preparing deep sleep");
  Serial.flush();
  const auto& input = BoardConfig::ACTIVE.input;
  const int8_t powerPin = input.power;
  const bool activeHigh = input.powerActiveHigh;
  bool released = powerPin < 0;
  if (powerPin >= 0) {
    pinMode(powerPin, activeHigh ? INPUT_PULLDOWN : INPUT_PULLUP);
    const int pressedLevel = activeHigh ? HIGH : LOW;
    const unsigned long started = millis();
    while (digitalRead(powerPin) == pressedLevel && millis() - started < 3000UL) delay(20);
    released = digitalRead(powerPin) != pressedLevel;
  }
  if (!released) {
    if (sleepOwner == tamaink::low_battery::Owner::LowBattery) lowBatterySleep.saveFailed();
    wakeDiagnostic.armFailed();
    wakeDiagnostic.requested = false;
    Serial.println(lowBatterySleepRequested ? "Low-battery sleep canceled: power-button release timeout" : "Wake diagnostic canceled: power-button release timeout");
    if (emulatorActive && !rendererEnabled) {
      if (startRenderer() && enqueueRenderer() == pdPASS)
        rendererDispatch.queued(millis());
    }
    return;
  }
  if (!freeink::PowerManager::armPowerButtonWakeup()) {
    if (sleepOwner == tamaink::low_battery::Owner::LowBattery) lowBatterySleep.saveFailed();
    wakeDiagnostic.armFailed();
    wakeDiagnostic.requested = false;
    Serial.println(lowBatterySleepRequested ? "Low-battery sleep canceled: power-button wake arm failed" : "Wake diagnostic canceled: power-button wake arm failed");
    return;
  }
  // Quiesce and join the renderer before touching its framebuffer.
  rendererStopRequested = true;
  if (rendererTaskHandle && rendererStopped) {
    if (xSemaphoreTake(rendererStopped, portMAX_DELAY) == pdTRUE) {
      vTaskDelete(rendererTaskHandle); rendererTaskHandle = nullptr;
    }
  }
  renderSleepScreenAndRelease();
  // Revalidate release and arm immediately before rail shutdown; a stale
  // button state must never leave the device showing a misleading terminal screen.
  bool releasedAgain = powerPin < 0;
  if (powerPin >= 0) {
    const int pressedLevel = activeHigh ? HIGH : LOW;
    const unsigned long checkStart = millis();
    while (digitalRead(powerPin) == pressedLevel && millis() - checkStart < 1000UL) delay(20);
    releasedAgain = digitalRead(powerPin) != pressedLevel;
  }
  if (!releasedAgain || !freeink::PowerManager::armPowerButtonWakeup()) {
    if (sleepOwner == tamaink::low_battery::Owner::LowBattery) lowBatterySleep.saveFailed();
    Serial.println(lowBatterySleepRequested ? "Low-battery sleep canceled: final GPIO3 release/arm failed; restoring renderer" : "Wake diagnostic canceled: final GPIO3 release/arm failed; restoring renderer");
    wakeDiagnostic.armFailed(); wakeDiagnostic.requested = false;
    if (emulatorActive && !rendererEnabled && startRenderer() &&
        enqueueRenderer() == pdPASS)
      rendererDispatch.queued(millis());
    return;
  }
  freeink::PowerManager::powerDownRailsForSleep();
  Serial.println(lowBatterySleepRequested ? "Low-battery sleep: armed GPIO3 power-button wake; entering deep sleep" : "Wake diagnostic: armed GPIO3 power-button wake; entering deep sleep");
  Serial.flush();
  freeink::PowerManager::deepSleep();
}

void cancelWakeDiagnosticSave() {
  if (wakeDiagnostic.pending) {
    wakeDiagnostic.saveFailed();
    if (wakeDiagnostic.requested) Serial.println("Wake diagnostic canceled: save failed");
    wakeDiagnostic.requested = false;
  }
  if (saveArbiter.owner() == tamaink::low_battery::Owner::LowBattery || lowBatterySleep.pending()) {
    lowBatterySleep.saveFailed(); saveArbiter.fail(tamaink::low_battery::Owner::LowBattery);
    Serial.println("Low-battery sleep canceled: save refused");
  }
  if (saveArbiter.owner() != tamaink::low_battery::Owner::None) saveArbiter.fail(saveArbiter.owner());
}

const char* wakeupCauseName(esp_sleep_wakeup_cause_t cause) {
  switch (cause) {
    case ESP_SLEEP_WAKEUP_UNDEFINED: return "undefined";
    case ESP_SLEEP_WAKEUP_EXT0: return "ext0";
    case ESP_SLEEP_WAKEUP_EXT1: return "ext1";
    case ESP_SLEEP_WAKEUP_TIMER: return "timer";
    case ESP_SLEEP_WAKEUP_TOUCHPAD: return "touchpad";
    case ESP_SLEEP_WAKEUP_ULP: return "ulp";
    case ESP_SLEEP_WAKEUP_GPIO: return "gpio";
    default: return "other";
  }
}

void persistenceCommand(char c, bool internal = false) {
  if (c != 'p' && c != 'n' && c != 'c' && c != 'x') return;
  if (!internal && c == 'p' && !saveArbiter.claim(tamaink::low_battery::Owner::Manual)) { Serial.println("Persistence command refused: save transaction owned"); return; }
  if (!internal && c == 'n' && saveArbiter.owner() != tamaink::low_battery::Owner::Manual) { Serial.println("Persistence command refused: not manual owner"); return; }
  if (!internal && (c == 'c' || c == 'x') && saveArbiter.owner() != tamaink::low_battery::Owner::None) { Serial.println("Persistence command refused: save transaction owned"); return; }
  if (!wakeDiagnostic.mutationAllowed(internal)) { if (c == 'p') cancelWakeDiagnosticSave(); Serial.println("Persistence command refused: wake diagnostic save pending"); return; }
  if (!internal && !autosaveController.manualAllowed(persistenceState == PersistenceState::AwaitReset)) { if (c == 'p') cancelWakeDiagnosticSave(); Serial.println(persistenceState == PersistenceState::AwaitReset ? "Persistence command refused: await physical reset" : "Persistence command refused: automatic save in progress"); return; }
  if (!emulatorActive) { if (c == 'p') cancelWakeDiagnosticSave(); Serial.println("Persistence command refused: emulator inactive"); return; }
  if (!persistenceReady) { if (c == 'p') cancelWakeDiagnosticSave(); Serial.println("Persistence command refused: SD unavailable"); return; }
  if (persistenceState == PersistenceState::AwaitReset) { if (c == 'p') cancelWakeDiagnosticSave(); Serial.println("Persistence command refused: await physical reset"); return; }
  if (c == 'p') {
    if (persistenceState != PersistenceState::Idle) {
      if (!internal) saveArbiter.fail(tamaink::low_battery::Owner::Manual);
      return;
    }
    if (!SdMan.ensureDirectoryExists(kPersistDir)) { cancelWakeDiagnosticSave(); Serial.println("Persistence p refused: directory create failed"); return; }
    const uint8_t target = persistenceHasSelected ? static_cast<uint8_t>(1 - persistenceSlot) : 0;
    const uint32_t next = persistenceHasSelected ? persistenceGeneration + 1u : 0u;
    tamaink::persist::Record& r = persistenceBootRecord[0]; r = {}; std::memcpy(r.rom, kPersistRom, 8); r.generation = next; std::uint64_t rtcEpoch = 0; r.timestamp = readRtcEpoch(rtcEpoch) ? tamaink::rtc::encodeTagged(rtcEpoch) : 0; if (!r.timestamp) Serial.println("Persistence: RTC unavailable/invalid; saving untagged timestamp");
    if (emulator.export_state(&persistenceBootState) != tamaink::tamalib::Status::Ok) { cancelWakeDiagnosticSave(); Serial.println("Persistence p refused: live export failed"); return; }
    r.payloadLength = tamaink::emulator::encode(persistenceBootState, r.payload, sizeof r.payload, kPersistRom);
    if (r.payloadLength != tamaink::emulator::kEncodedSize) { cancelWakeDiagnosticSave(); Serial.println("Persistence p refused: state encode failed"); return; }
    persistenceRecordSize = tamaink::persist::encode(r, persistenceRecord, sizeof persistenceRecord);
    if (persistenceRecordSize == 0) { cancelWakeDiagnosticSave(); Serial.println("Persistence p refused: encode failed"); return; }
    std::memcpy(persistenceExpected, persistenceRecord, persistenceRecordSize);
    if (!tamaink::persist::stageEncodedRecord(persistenceRecord, persistenceRecordSize)) { cancelWakeDiagnosticSave(); Serial.println("Persistence p refused: staging failed"); return; }
    persistencePendingSlot = target; persistencePendingGeneration = next;
    persistenceFile = SdMan.open(kPersistPaths[target], O_RDWR | O_CREAT | O_TRUNC);
    if (!persistenceFile) { failPersistenceWrite("open"); return; }
    persistenceState = PersistenceState::Open;
    Serial.printf("Persistence p: target slot %c generation=%lu; reset now or send n for Partial\n", 'A' + target, static_cast<unsigned long>(next)); return;
  }
  if (c == 'n') {
    if (persistenceState == PersistenceState::Idle) return;
    const size_t partial = tamaink::persist::kHeaderSize / 2;
    if (persistenceState == PersistenceState::Open) { if (persistenceFile.write(persistenceRecord, partial) != partial) { failPersistenceWrite("partial write"); return; } persistenceState = PersistenceState::Partial; Serial.println(internal ? "Autosave phase: partial" : "Persistence Partial complete; reset now or send n"); }
    else if (persistenceState == PersistenceState::Partial) { const size_t rem = persistenceRecordSize - partial; if (persistenceFile.write(persistenceRecord + partial, rem) != rem) { failPersistenceWrite("remainder write"); return; } persistenceState = PersistenceState::Complete; Serial.println(internal ? "Autosave phase: complete" : "Persistence CompleteUnsynced; reset now or send n"); }
    else if (persistenceState == PersistenceState::Complete) { if (!persistenceFile.sync()) { failPersistenceWrite("sync"); return; } persistenceState = PersistenceState::Synced; Serial.println(internal ? "Autosave phase: synced" : "Persistence Synced; reset now or send n"); }
    else if (persistenceState == PersistenceState::Synced) { persistenceFile.close(); if (!verifyStagedSlot(persistencePendingSlot)) { failPersistenceWrite("staged validation"); return; } persistenceState = PersistenceState::Verified; Serial.println(internal ? "Autosave phase: verified" : "Persistence Verified staged; reset now or send n to commit"); }
    else if (persistenceState == PersistenceState::Verified) {
      persistenceFile = SdMan.open(kPersistPaths[persistencePendingSlot], O_RDWR);
      if (!persistenceFile || !persistenceFile.seek(tamaink::persist::kCrcOffset) ||
          persistenceFile.write(persistenceExpected + tamaink::persist::kCrcOffset,
                                tamaink::persist::kCrcSize) != tamaink::persist::kCrcSize ||
          !persistenceFile.sync()) {
        failPersistenceWrite("commit");
        return;
      }
      persistenceFile.close();
      if (!readPersistenceSlot(persistencePendingSlot, persistenceBootRecord[0]) ||
          !payloadMatches(persistenceBootRecord[0]) ||
          persistenceBootRecord[0].generation != persistencePendingGeneration) {
        Serial.println("Persistence commit rejected: reread validation failed; prior slot retained");
        persistenceState = PersistenceState::Idle;
        wakeDiagnostic.saveFailed();
        if (saveArbiter.owner() == tamaink::low_battery::Owner::LowBattery) lowBatterySleep.saveFailed();
        saveArbiter.fail(saveArbiter.owner());
        if (wakeDiagnostic.requested) Serial.println("Wake diagnostic canceled: save failed");
        wakeDiagnostic.requested = false;
        autosaveController.commitRereadFailure(millis(), AUTOSAVE_RETRY_MS);
        scanPersistence();
        return;
      }
      persistenceSlot = persistencePendingSlot;
      persistenceGeneration = persistencePendingGeneration;
      persistenceHasSelected = true;
      persistenceState = PersistenceState::Idle;
      autosaveController.successCommit(millis(), autosaveInterval());
      Serial.println("Persistence commit complete and verified");
      const auto owner = saveArbiter.owner();
      if (owner == tamaink::low_battery::Owner::WakeDiagnostic) {
        const bool sleepReady = wakeDiagnostic.saveSucceeded();
        wakeDiagnostic.requested = false;
        saveArbiter.complete(owner);
        if (sleepReady) enterWakeDiagnosticSleep(owner);
      } else if (owner == tamaink::low_battery::Owner::LowBattery) {
        lowBatterySleep.saveSucceeded();
        saveArbiter.complete(owner);
        enterWakeDiagnosticSleep(owner);
      } else {
        saveArbiter.complete(owner);
      }
    }
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
  if (c == 'x') { const bool a = !SdMan.exists(kPersistPaths[0]) || SdMan.remove(kPersistPaths[0]); const bool b = !SdMan.exists(kPersistPaths[1]) || SdMan.remove(kPersistPaths[1]); if (!a || !b) { Serial.println("Persistence cleanup failed"); scanPersistence(); return; } if (SdMan.exists(kPersistDir)) SdMan.rmdir(kPersistDir); persistenceHasSelected = false; persistenceGeneration = 0; persistenceSlot = 0; autosaveController.cleanup(millis(), autosaveInterval()); Serial.println("Persistence owned files cleaned"); }
}

void dispatchSerialCommand(char c) {
  if (c == 'B') {
    if (!emulatorActive) { Serial.println("Battery diagnostic B refused: emulator inactive"); return; }
    injectLowBatterySample();
    return;
  }
  if (c == 'b') {
    if (!emulatorActive) { Serial.println("Battery telemetry refused: emulator inactive"); return; }
    if (sampleBatteryTelemetry("manual")) batteryTelemetrySchedule.arm(millis());
    return;
  }
  if (c == 'w') {
    if (!saveArbiter.claim(tamaink::low_battery::Owner::WakeDiagnostic)) { Serial.println("Wake diagnostic refused: save transaction owned"); return; }
    if (!wakeDiagnostic.request(emulatorActive, persistenceReady, persistenceState == PersistenceState::Idle, autosaveController.automatic, persistenceState == PersistenceState::AwaitReset)) { saveArbiter.fail(tamaink::low_battery::Owner::WakeDiagnostic); Serial.println("Wake diagnostic refused: emulator/SD busy, await reset, or request pending"); return; }
    tamaink::autosave::request(autosaveController.scheduler); Serial.println("Wake diagnostic requested: durable save will begin"); return;
  }
  if (c == 'a') {
    if (!saveArbiter.claim(tamaink::low_battery::Owner::Automatic)) { Serial.println("Autosave request refused: save transaction owned"); return; }
    if (wakeDiagnostic.pending) { saveArbiter.fail(tamaink::low_battery::Owner::Automatic); Serial.println("Autosave request refused: wake diagnostic save pending"); return; }
    if (persistenceState != PersistenceState::Idle || autosaveController.automatic) { saveArbiter.fail(tamaink::low_battery::Owner::Automatic); Serial.println("Autosave request refused: transaction busy"); return; }
    tamaink::autosave::request(autosaveController.scheduler); Serial.println("Autosave requested"); return;
  }
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
  settingsPrefs.begin("tamaink-ui", false);
  std::uint8_t persisted[8]{}; const std::size_t n = settingsPrefs.getBytes("values", persisted, sizeof persisted); tamaink::settings::Values loaded{}; if (n==5 && tamaink::settings::decode(persisted,n,loaded)) settingsController.setValues(loaded);

  const esp_reset_reason_t resetReason = esp_reset_reason();
  Serial.printf("TamaInk %s\n", TAMAINK_VERSION);
  Serial.printf("Reset reason: %s (%d)\n", resetReasonName(resetReason), static_cast<int>(resetReason));
  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  Serial.printf("Wakeup cause: %s (%d)\n", wakeupCauseName(wakeCause), static_cast<int>(wakeCause));
  if (wakeCause == ESP_SLEEP_WAKEUP_GPIO) {
    Serial.printf("GPIO wake status: 0x%llX\n", static_cast<unsigned long long>(esp_sleep_get_gpio_wakeup_status()));
  }

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
      const bool hadDeferredWakeCatchup = wakeCatchupPending;
      const bool rendererStarted = startRenderer();
      if (rendererStarted) {
        if (enqueueRenderer() == pdPASS) rendererDispatch.queued(millis());
        if (hadDeferredWakeCatchup && rendererFirstFrameDone &&
            xSemaphoreTake(rendererFirstFrameDone, pdMS_TO_TICKS(13000)) != pdTRUE)
          Serial.println("Display renderer: first-frame completion timed out; continuing wake catch-up");
        if (hadDeferredWakeCatchup && !rendererFirstFrameConfirmed)
          Serial.println("Display renderer: imported first frame was not confirmed; continuing wake catch-up");
      }
      if (hadDeferredWakeCatchup) runDeferredWakeCatchup();
      if (hadDeferredWakeCatchup && rendererStarted && rendererEnabled &&
          enqueueRenderer() == pdPASS)
        rendererDispatch.queued(millis());
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
    const uint32_t nowTelemetry = millis();
    if (batteryTelemetrySchedule.due(nowTelemetry)) {
      if (sampleBatteryTelemetry("periodic")) batteryTelemetrySchedule.arm(nowTelemetry);
    }
    const uint32_t nowAuto = millis();
    if (autosaveController.tick(nowAuto, persistenceState == PersistenceState::Idle, persistenceState == PersistenceState::AwaitReset) == tamaink::autosave::Action::Begin) {
      Serial.println("Autosave: starting staged transaction");
      if (saveArbiter.owner() == tamaink::low_battery::Owner::None) saveArbiter.claim(tamaink::low_battery::Owner::Automatic);
      persistenceCommand('p', true);
      if (persistenceState == PersistenceState::Idle) {
        cancelWakeDiagnosticSave();
        autosaveController.beginFailure(nowAuto, AUTOSAVE_RETRY_MS);
      }
    } else if (autosaveController.advance(persistenceState == PersistenceState::Idle) == tamaink::autosave::Action::Advance) {
      persistenceCommand('n', true);
    }
    emulator.step(64, &emulatorSnapshot);
    const bool lcdChanged = !emulatorObservedValid ||
        std::memcmp(emulatorSnapshot.lcd, emulatorObserved.lcd, sizeof emulatorSnapshot.lcd) != 0;
    const bool iconChanged = !emulatorObservedValid || emulatorSnapshot.icons != emulatorObserved.icons;
    if (lcdChanged || iconChanged) {
      emulatorObserved = emulatorSnapshot;
      emulatorObservedValid = true;
      serialFramePending = serialLcdFramesEnabled;
      if (lcdChanged || iconChanged) rendererDispatch.changed(iconChanged, millis());
    }
    const unsigned long now = millis();
    if (serialLcdFramesEnabled && serialFramePending && now - emulatorLastPrintAt >= EMULATOR_SERIAL_FRAME_INTERVAL_MS) {
      printEmulatorSnapshot(emulatorSnapshot);
      emulatorPrinted = emulatorSnapshot;
      emulatorPrintedValid = true;
      serialFramePending = false;
      emulatorLastPrintAt = now;
    }
    if (rendererEnabled && rendererDispatch.eligible(now) && enqueueRenderer() == pdPASS)
      rendererDispatch.queued(now);
    delay(0);
    return;
  }
  if (inputReady) updateInputDiagnostic();
  while (Serial.available()) dispatchSerialCommand(static_cast<char>(Serial.read()));
  delay(10);
}
