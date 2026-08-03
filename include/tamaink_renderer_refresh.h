#pragma once

#include <cstdint>

namespace tamaink::render {

enum class RefreshKind : std::uint8_t { Full, Fast };

constexpr bool preserveFullRefresh(bool queued, bool requested) {
  return queued || requested;
}

// Selects a full refresh initially and after threshold successfully presented
// fast frames. Rejected or unpresented frames must not call presented().
class RefreshCadence {
 public:
  static constexpr std::uint8_t kFastFramesBeforeFull = 64;

  explicit RefreshCadence(std::uint8_t threshold = kFastFramesBeforeFull)
      : threshold_(threshold ? threshold : 1) {}
  RefreshKind next() const;
  void presented(RefreshKind kind);
  std::uint8_t fastFrames() const { return fastFrames_; }
  std::uint8_t threshold() const { return threshold_; }
  void setThreshold(std::uint8_t threshold) { threshold_ = threshold ? threshold : 1; }

 private:
  bool initial_ = true;
  std::uint8_t fastFrames_ = 0;
  std::uint8_t threshold_ = kFastFramesBeforeFull;
};

}  // namespace tamaink::render
