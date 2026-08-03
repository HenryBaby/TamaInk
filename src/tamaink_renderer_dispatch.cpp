#include "tamaink_renderer_dispatch.h"

namespace tamaink::render {

void FrameDispatch::changed(bool icon, std::uint32_t now) {
  pending_ = true;
  urgent_ = urgent_ || icon;
  (void)now;
}

bool FrameDispatch::eligible(std::uint32_t now) const {
  return pending_ && (urgent_ || !hasQueued_ || static_cast<std::uint32_t>(now - lastQueued_) >= intervalMs_);
}

void FrameDispatch::queued(std::uint32_t now) {
  pending_ = false;
  urgent_ = false;
  hasQueued_ = true;
  lastQueued_ = now;
}

}  // namespace tamaink::render
