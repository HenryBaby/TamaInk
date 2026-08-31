#include "tamaink_board_policy.h"
#include <cassert>
#include <cstring>

int main() {
  using namespace tamaink::board;
  assert(validate(Family::X3, Controller::Uc8253).state == State::Supported);
  assert(validate(Family::X4, Controller::Ssd1677).state == State::Supported);
  assert(validate(Family::X3, Controller::Uc8279d).state == State::Supported);
  assert(validate(Family::X4, Controller::Uc8179).state == State::Supported);
  assert(validate(Family::X4, Controller::Uc8279).state == State::Supported);
  assert(validate(Family::X3, Controller::Ssd1677).state == State::InvalidCombination);
  assert(validate(Family::X3, Controller::Uc8179).state == State::InvalidCombination);
  assert(validate(Family::X3, Controller::Uc8279).state == State::InvalidCombination);
  assert(validate(Family::X4, Controller::Uc8253).state == State::InvalidCombination);
  assert(validate(Family::X4, Controller::Uc8279d).state == State::InvalidCombination);
  assert(validate(Family::X3, Controller::Unknown).state == State::InvalidCombination);
  assert(validate(Family::X4, Controller::Unknown).state == State::InvalidCombination);
  assert(validate(Family::X3, Controller::Uc8253).accepted());
  assert(validate(Family::X3, Controller::Uc8279d).accepted());
  assert(validate(Family::X4, Controller::Ssd1677).accepted());
  assert(validate(Family::X4, Controller::Uc8179).accepted());
  assert(validate(Family::X4, Controller::Uc8279).accepted());
  assert(std::strstr(validate(Family::X3, Controller::Uc8279d).message, "X3 UC8279d") != nullptr);
  assert(std::strstr(validate(Family::X4, Controller::Uc8279).message, "X4 UC8279") != nullptr);
  return 0;
}
