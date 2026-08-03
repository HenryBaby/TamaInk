#pragma once
#include <cstddef>
#include <cstdint>

namespace tamaink::settings {

enum class Battery : std::uint8_t { Show, Hide };
enum class Display : std::uint8_t { Smooth, Balanced, Eco };
enum class Autosave : std::uint8_t { Min5, Min15, Min30 };

struct Values {
  Battery battery = Battery::Show;
  Display display = Display::Balanced;
  Autosave autosave = Autosave::Min15;
};

Values defaults();
bool validate(const Values& values);
std::uint32_t autosaveIntervalMs(Autosave autosave);
std::uint32_t displayIntervalMs(Display display);
std::uint8_t cleaningThreshold(Display display);
std::size_t encode(const Values& values, std::uint8_t* output, std::size_t capacity);
bool decode(const std::uint8_t* data, std::size_t length, Values& output);

class Controller {
 public:
  enum class Event : std::uint8_t { None, Opened, Changed, Closed, DialogOpened, ResetRequested };
  Event update(bool upEdge, bool backEdge, bool confirmEdge, bool powerEdge);
  bool open() const { return open_; }
  std::uint8_t focus() const { return focus_; }
  bool confirmation() const { return confirmation_; }
  bool resetYes() const { return resetYes_; }
  const Values& values() const { return values_; }
  Values& values() { return values_; }
  void setValues(const Values& values) {
    if (validate(values)) values_ = values;
  }

 private:
  bool open_ = false;
  bool confirmation_ = false;
  bool resetYes_ = false;
  std::uint8_t focus_ = 0;
  Values values_{};
};

}  // namespace tamaink::settings
