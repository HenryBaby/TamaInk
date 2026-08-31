#pragma once

#include "tamaink_board_policy.h"

namespace tamaink::display_variant {

// The SDK deliberately exposes UC8279 for both regular X3 and X4 panels. Keep
// that ambiguity at this boundary, where the detected physical family is known.
enum class DetectedController { Uc8253, Ssd1677, Uc8279, Uc8179, Unknown };
enum class ProfileIntent { X3, X3Uc8279, X4, Invalid };

struct Resolution {
  board::Controller controller;
  ProfileIntent profile;
  bool selectX3Panel;

  constexpr bool valid() const { return profile != ProfileIntent::Invalid; }
};

constexpr Resolution resolve(board::Family family, DetectedController detected) {
  if (family == board::Family::X3) {
    if (detected == DetectedController::Uc8253)
      return {board::Controller::Uc8253, ProfileIntent::X3, true};
    if (detected == DetectedController::Uc8279)
      return {board::Controller::Uc8279d, ProfileIntent::X3Uc8279, true};
    return {board::Controller::Unknown, ProfileIntent::Invalid, false};
  }
  if (family == board::Family::X4) {
    if (detected == DetectedController::Ssd1677)
      return {board::Controller::Ssd1677, ProfileIntent::X4, false};
    if (detected == DetectedController::Uc8179)
      return {board::Controller::Uc8179, ProfileIntent::X4, false};
    if (detected == DetectedController::Uc8279)
      return {board::Controller::Uc8279, ProfileIntent::X4, false};
    return {board::Controller::Unknown, ProfileIntent::Invalid, false};
  }
  return {board::Controller::Unknown, ProfileIntent::Invalid, false};
}

}  // namespace tamaink::display_variant
