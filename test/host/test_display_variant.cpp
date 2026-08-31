#include "tamaink_display_variant.h"
#include <cassert>
#include <initializer_list>

int main() {
  using namespace tamaink;
  using display_variant::DetectedController;
  using display_variant::ProfileIntent;
  using board::Controller;
  using board::Family;

  const auto x3 = display_variant::resolve(Family::X3, DetectedController::Uc8253);
  assert(x3.valid() && x3.controller == Controller::Uc8253 && x3.profile == ProfileIntent::X3 && x3.selectX3Panel);
  const auto x3uc = display_variant::resolve(Family::X3, DetectedController::Uc8279);
  assert(x3uc.valid() && x3uc.controller == Controller::Uc8279d && x3uc.profile == ProfileIntent::X3Uc8279 && x3uc.selectX3Panel);

  const auto x4 = display_variant::resolve(Family::X4, DetectedController::Ssd1677);
  assert(x4.valid() && x4.controller == Controller::Ssd1677 && x4.profile == ProfileIntent::X4 && !x4.selectX3Panel);
  const auto x4uc8179 = display_variant::resolve(Family::X4, DetectedController::Uc8179);
  assert(x4uc8179.valid() && x4uc8179.controller == Controller::Uc8179 && x4uc8179.profile == ProfileIntent::X4 && !x4uc8179.selectX3Panel);
  const auto x4uc = display_variant::resolve(Family::X4, DetectedController::Uc8279);
  assert(x4uc.valid() && x4uc.controller == Controller::Uc8279 && x4uc.profile == ProfileIntent::X4 && !x4uc.selectX3Panel);

  for (const auto family : {Family::X3, Family::X4}) {
    for (const auto detected : {DetectedController::Uc8253, DetectedController::Ssd1677,
                                DetectedController::Uc8279, DetectedController::Uc8179,
                                DetectedController::Unknown}) {
      const auto result = display_variant::resolve(family, detected);
      const bool expected = (family == Family::X3 &&
                             (detected == DetectedController::Uc8253 || detected == DetectedController::Uc8279)) ||
                            (family == Family::X4 &&
                             (detected == DetectedController::Ssd1677 || detected == DetectedController::Uc8279 ||
                              detected == DetectedController::Uc8179));
      assert(result.valid() == expected);
    }
  }
  return 0;
}
