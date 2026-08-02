#include "tamaink_renderer_refresh.h"

namespace tamaink::render {

RefreshKind RefreshCadence::next() const {
  return initial_ || fastFrames_ >= kFastFramesBeforeFull ? RefreshKind::Full : RefreshKind::Fast;
}

void RefreshCadence::presented(RefreshKind kind) {
  if (kind == RefreshKind::Full) {
    initial_ = false;
    fastFrames_ = 0;
  } else if (fastFrames_ < kFastFramesBeforeFull) {
    ++fastFrames_;
  }
}

}  // namespace tamaink::render
