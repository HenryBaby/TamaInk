#pragma once

namespace tamaink::board {

enum class Family { X3, X4 };
enum class Controller { Uc8253, Ssd1677, Uc8279d, Uc8179, Unknown };
enum class State { Supported, DisabledPendingValidation, InvalidCombination };

struct Decision {
  State state;
  const char* message;

  constexpr bool accepted() const { return state == State::Supported; }
};

struct Rule { Family family; Controller controller; State state; const char* message; };

// The single policy table is intentionally the only enablement gate. Promoting
// a validated controller later is one status change plus its focused test.
constexpr Rule kRules[] = {
    {Family::X3, Controller::Uc8253, State::Supported, "X3 UC8253 accepted"},
    {Family::X4, Controller::Ssd1677, State::Supported, "X4 SSD1677 accepted (display renderer pending)"},
    {Family::X3, Controller::Uc8279d, State::DisabledPendingValidation, "X3 UC8279d disabled pending validation"},
    {Family::X4, Controller::Uc8179, State::DisabledPendingValidation, "X4 UC8179 disabled pending validation"},
};

constexpr Decision validate(Family family, Controller controller) {
  for (const auto& rule : kRules)
    if (rule.family == family && rule.controller == controller)
      return {rule.state, rule.message};
  return {State::InvalidCombination, "board/controller combination invalid"};
}

}  // namespace tamaink::board
