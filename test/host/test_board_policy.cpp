#include "tamaink_board_policy.h"
#include <cassert>
#include <cstring>

int main() {
  using namespace tamaink::board;
  assert(validate(Family::X3, Controller::Uc8253).state == State::Supported);
  assert(validate(Family::X4, Controller::Ssd1677).state == State::Supported);
  assert(validate(Family::X3, Controller::Uc8279d).state == State::DisabledPendingValidation);
  assert(validate(Family::X4, Controller::Uc8179).state == State::DisabledPendingValidation);
  assert(validate(Family::X3, Controller::Unknown).state == State::InvalidCombination);
  assert(validate(Family::X3, Controller::Uc8253).accepted());
  assert(!validate(Family::X3, Controller::Uc8279d).accepted());
  assert(std::strstr(validate(Family::X3, Controller::Uc8279d).message, "UC8279d") != nullptr);
  assert(std::strstr(validate(Family::X4, Controller::Uc8179).message, "UC8179") != nullptr);
  assert(std::strstr(validate(Family::X4, Controller::Ssd1677).message, "pending") != nullptr);
  return 0;
}
