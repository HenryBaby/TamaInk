#include "tamaink_settings.h"

namespace tamaink::settings {

Values defaults() { return {}; }

bool validate(const Values& values) {
  return values.battery <= Battery::Hide && values.display <= Display::Eco &&
         values.autosave <= Autosave::Min30;
}

std::uint32_t autosaveIntervalMs(Autosave autosave) {
  switch (autosave) {
    case Autosave::Min5: return 5UL * 60UL * 1000UL;
    case Autosave::Min30: return 30UL * 60UL * 1000UL;
    default: return 15UL * 60UL * 1000UL;
  }
}

std::uint32_t displayIntervalMs(Display display) {
  switch (display) {
    case Display::Smooth: return 750;
    case Display::Eco: return 3000;
    default: return 1500;
  }
}

std::uint8_t cleaningThreshold(Display display) {
  switch (display) {
    case Display::Smooth: return 32;
    case Display::Eco: return 128;
    default: return 64;
  }
}

std::size_t encode(const Values& values, std::uint8_t* output, std::size_t capacity) {
  if (!output || capacity < 5 || !validate(values)) return 0;
  output[0] = 'T'; output[1] = 'S'; output[2] = 1;
  output[3] = static_cast<std::uint8_t>(values.battery);
  output[4] = static_cast<std::uint8_t>(static_cast<std::uint8_t>(values.display) << 4) |
              static_cast<std::uint8_t>(values.autosave);
  return 5;
}

bool decode(const std::uint8_t* data, std::size_t length, Values& output) {
  if (!data || length != 5 || data[0] != 'T' || data[1] != 'S' || data[2] != 1) return false;
  Values values{static_cast<Battery>(data[3]), static_cast<Display>(data[4] >> 4),
                static_cast<Autosave>(data[4] & 0x0f)};
  if (!validate(values)) return false;
  output = values;
  return true;
}

Controller::Event Controller::update(std::uint32_t now, bool up, bool back,
                                     bool confirm, bool power) {
  if (!open_) {
    if (upNeedsRelease_) {
      if (!up) upNeedsRelease_ = false;
      return Event::None;
    }
    if (up) {
      if (!upTracking_) { upTracking_ = true; upSince_ = now; }
      if (static_cast<std::uint32_t>(now - upSince_) >= 1500u) {
        open_ = true;
        focus_ = 0;
        upTracking_ = false;
        upNeedsRelease_ = true;
        return Event::Opened;
      }
    } else {
      upTracking_ = false;
    }
    return Event::None;
  }
  if (power) { open_ = false; return Event::Closed; }
  if (back) { focus_ = static_cast<std::uint8_t>((focus_ + 1) % 4); return Event::Changed; }
  if (confirm) {
    if (focus_ == 3) { open_ = false; return Event::Closed; }
    if (focus_ == 0) values_.battery = values_.battery == Battery::Show ? Battery::Hide : Battery::Show;
    else if (focus_ == 1) values_.display = static_cast<Display>((static_cast<int>(values_.display) + 1) % 3);
    else values_.autosave = static_cast<Autosave>((static_cast<int>(values_.autosave) + 1) % 3);
    return Event::Changed;
  }
  return Event::None;
}

}  // namespace tamaink::settings
