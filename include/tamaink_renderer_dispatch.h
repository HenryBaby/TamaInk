#pragma once

#include <cstdint>

namespace tamaink::render {

class FrameDispatch {
 public:
  static constexpr std::uint32_t kLcdIntervalMs = 1500;
  void changed(bool icon, std::uint32_t now);
  bool eligible(std::uint32_t now) const;
  void queued(std::uint32_t now);
  void reset() { pending_ = false; urgent_ = false; hasQueued_ = false; lastQueued_ = 0; }
  bool pending() const { return pending_; }

 private:
  bool pending_ = false;
  bool urgent_ = false;
  bool hasQueued_ = false;
  std::uint32_t lastQueued_ = 0;
};

}  // namespace tamaink::render
