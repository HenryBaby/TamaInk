#include "tamaink_settings.h"

#include <cassert>

using namespace tamaink::settings;

int main() {
  Controller menu;
  assert(!menu.open());
  assert(menu.update(false, false, false, false) == Controller::Event::None);
  assert(menu.update(true, false, false, false) == Controller::Event::Opened);
  assert(menu.open() && menu.focus() == 0);

  // Battery cycles SHOW -> HIDE -> SHOW.
  assert(menu.update(false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().battery == Battery::Hide);
  assert(menu.update(false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().battery == Battery::Show);

  // BACK advances focus; each setting wraps through all available values.
  assert(menu.update(false, true, false, false) == Controller::Event::Changed);
  assert(menu.focus() == 1);
  assert(menu.update(false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().display == Display::Eco);
  assert(menu.update(false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().display == Display::Smooth);
  assert(menu.update(false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().display == Display::Balanced);

  assert(menu.update(false, true, false, false) == Controller::Event::Changed);
  assert(menu.focus() == 2);
  assert(menu.update(false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().autosave == Autosave::Min30);
  assert(menu.update(false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().autosave == Autosave::Min5);
  assert(menu.update(false, false, true, false) == Controller::Event::Changed);
  assert(menu.values().autosave == Autosave::Min15);

  assert(menu.update(false, true, false, false) == Controller::Event::Changed);
  assert(menu.focus() == 3);
  assert(menu.update(false, false, true, false) == Controller::Event::DialogOpened);
  assert(menu.confirmation() && !menu.resetYes());
  assert(menu.update(false, false, true, false) == Controller::Event::Changed);
  assert(menu.open() && !menu.confirmation());
  assert(menu.focus() == 3);
  assert(menu.update(false, false, true, false) == Controller::Event::DialogOpened);
  assert(menu.update(false, true, false, false) == Controller::Event::Changed);
  assert(menu.resetYes());
  assert(menu.update(false, false, true, false) == Controller::Event::ResetRequested);
  assert(!menu.open());

  // POWER safely cancels from the confirmation dialog without requesting reset.
  assert(menu.update(true, false, false, false) == Controller::Event::Opened);
  assert(menu.update(false, true, false, false) == Controller::Event::Changed);
  assert(menu.update(false, true, false, false) == Controller::Event::Changed);
  assert(menu.update(false, true, false, false) == Controller::Event::Changed);
  assert(menu.update(false, false, true, false) == Controller::Event::DialogOpened);
  assert(menu.update(false, false, false, true) == Controller::Event::Closed);
  assert(!menu.open() && !menu.confirmation());

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
  return 0;
}
