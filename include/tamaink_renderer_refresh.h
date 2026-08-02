#pragma once

#include <cstdint>

namespace tamaink::render {

enum class RefreshKind : std::uint8_t { Full, Fast };

// Selects a full refresh initially and after 64 successfully presented
// fast frames. Rejected or unpresented frames must not call presented().
class RefreshCadence {
 public:
  static constexpr std::uint8_t kFastFramesBeforeFull = 64;

  RefreshKind next() const;
  void presented(RefreshKind kind);
  std::uint8_t fastFrames() const { return fastFrames_; }

 private:
  bool initial_ = true;
  std::uint8_t fastFrames_ = 0;
};

}  // namespace tamaink::render
