#include "tamaink_settings.h"

#include <cassert>
#include <cstdint>

using namespace tamaink::settings;

int main() {
  // A short UP press remains unused and does not open the menu.
  Controller shortPress;
  assert(shortPress.update(10, true, false, false, false) == Controller::Event::None);
  assert(shortPress.update(1400, false, false, false, false) == Controller::Event::None);
  assert(!shortPress.open());

  Controller menu;
  assert(menu.update(0, true, false, false, false) == Controller::Event::None);
  assert(menu.update(1499, true, false, false, false) == Controller::Event::None);
  assert(menu.update(1500, true, false, false, false) == Controller::Event::Opened);
  assert(menu.open() && menu.focus() == 0);

  // Battery cycles SHOW -> HIDE -> SHOW.
  assert(menu.update(1501, false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().battery == Battery::Hide);
  assert(menu.update(1502, false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().battery == Battery::Show);

  // BACK advances focus; each setting wraps through all available values.
  assert(menu.update(1503, false, true, false, false) == Controller::Event::Changed);
  assert(menu.focus() == 1);
  assert(menu.update(1504, false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().display == Display::Eco);
  assert(menu.update(1505, false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().display == Display::Smooth);
  assert(menu.update(1506, false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().display == Display::Balanced);

  assert(menu.update(1507, false, true, false, false) == Controller::Event::Changed);
  assert(menu.focus() == 2);
  assert(menu.update(1508, false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().autosave == Autosave::Min30);
  assert(menu.update(1509, false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().autosave == Autosave::Min5);
  assert(menu.update(1510, false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().autosave == Autosave::Min15);

  assert(menu.update(1511, false, true, false, false) == Controller::Event::Changed);
  assert(menu.focus() == 3);
  assert(menu.update(1512, false, false, true, false) == Controller::Event::Closed);
  assert(!menu.open());

  // The opening button must be released before another hold can begin.
  assert(menu.update(4000, true, false, false, false) == Controller::Event::None);
  assert(menu.update(4001, false, false, false, false) == Controller::Event::None);
  assert(menu.update(4002, true, false, false, false) == Controller::Event::None);
  assert(menu.update(5502, true, false, false, false) == Controller::Event::Opened);
  assert(menu.update(5503, false, false, false, true) == Controller::Event::Closed);

  Values values{};
  std::uint8_t encoded[5]{};
  assert(encode(values, encoded, sizeof encoded) == sizeof encoded);
  Values decoded{};
  assert(decode(encoded, sizeof encoded, decoded));
  assert(decoded.battery == Battery::Show && decoded.display == Display::Balanced &&
         decoded.autosave == Autosave::Min15);
  encoded[2] = 9;
  assert(!decode(encoded, sizeof encoded, decoded));
  encoded[2] = 1;
  encoded[3] = 2;
  assert(!decode(encoded, sizeof encoded, decoded));
  encoded[3] = 0;
  encoded[4] = 0x30;
  assert(!decode(encoded, sizeof encoded, decoded));
  encoded[4] = 0x03;
  assert(!decode(encoded, sizeof encoded, decoded));
  assert(!decode(nullptr, sizeof encoded, decoded));
  assert(!decode(encoded, sizeof encoded - 1, decoded));

  assert(autosaveIntervalMs(Autosave::Min5) == 300000u);
  assert(autosaveIntervalMs(Autosave::Min15) == 900000u);
  assert(autosaveIntervalMs(Autosave::Min30) == 1800000u);
  assert(displayIntervalMs(Display::Smooth) == 750u);
  assert(displayIntervalMs(Display::Balanced) == 1500u);
  assert(displayIntervalMs(Display::Eco) == 3000u);
  assert(cleaningThreshold(Display::Smooth) == 32u);
  assert(cleaningThreshold(Display::Balanced) == 64u);
  assert(cleaningThreshold(Display::Eco) == 128u);

  // Unsigned subtraction keeps long-hold timing correct across millis rollover.
  Controller rollover;
  const std::uint32_t start = 0xfffffc00u;
  assert(rollover.update(start, true, false, false, false) == Controller::Event::None);
  assert(rollover.update(start + 1499u, true, false, false, false) == Controller::Event::None);
  assert(rollover.update(start + 1500u, true, false, false, false) == Controller::Event::Opened);
  return 0;
}
