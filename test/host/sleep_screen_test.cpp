#include "tamaink_sleep_screen.h"

#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
constexpr int kNativeWidth = 792;
constexpr int kNativeHeight = 528;
constexpr std::size_t kStride = 99;
constexpr std::size_t kBytes = kStride * kNativeHeight;

bool nativeBlack(const std::vector<std::uint8_t>& frame, int nativeX, int nativeY) {
  return (frame[static_cast<std::size_t>(nativeY) * kStride + static_cast<std::size_t>(nativeX) / 8u] &
          (0x80u >> (nativeX & 7))) == 0;
}

bool logicalBlack(const std::vector<std::uint8_t>& frame, int logicalX, int logicalY) {
  return nativeBlack(frame, logicalY, 527 - logicalX);
}

struct InkBounds {
  int left = 528;
  int top = 792;
  int right = -1;
  int bottom = -1;
  std::size_t pixels = 0;
};

InkBounds bounds(const std::vector<std::uint8_t>& frame, int left, int top, int right, int bottom) {
  InkBounds result{};
  for (int y = top; y <= bottom; ++y) {
    for (int x = left; x <= right; ++x) {
      if (!logicalBlack(frame, x, y)) continue;
      if (x < result.left) result.left = x;
      if (x > result.right) result.right = x;
      if (y < result.top) result.top = y;
      if (y > result.bottom) result.bottom = y;
      ++result.pixels;
    }
  }
  return result;
}

std::uint64_t fnv1a(const std::vector<std::uint8_t>& frame) {
  std::uint64_t hash = 1469598103934665603ull;
  for (std::uint8_t byte : frame) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

void expectRejectedWithoutWrite(tamaink::sleep_screen::Status expected, std::uint16_t width,
                                std::uint16_t height, std::size_t stride, std::size_t capacity) {
  std::vector<std::uint8_t> sentinel(kBytes, 0xA5);
  assert(tamaink::sleep_screen::render(sentinel.data(), capacity, width, height, stride) == expected);
  for (std::uint8_t byte : sentinel) assert(byte == 0xA5);
}
}  // namespace

int main() {
  using tamaink::sleep_screen::Status;
  std::vector<std::uint8_t> frame(kBytes, 0);
  assert(tamaink::sleep_screen::render(frame.data(), frame.size(), kNativeWidth, kNativeHeight, kStride) ==
         Status::Ok);

  // A golden framebuffer catches changes to wording, casing, weight, layout,
  // logo, and native orientation in one deterministic assertion.
  assert(fnv1a(frame) == 0x27B0653CE7620064ull);
  for (int nativeY : {0, kNativeHeight - 1}) {
    for (int nativeX : {0, kNativeWidth - 1}) assert(!nativeBlack(frame, nativeX, nativeY));
  }

  const InkBounds logo = bounds(frame, 180, 120, 348, 235);
  const InkBounds title = bounds(frame, 100, 250, 428, 325);
  const InkBounds sleeping = bounds(frame, 100, 340, 428, 395);
  assert(logo.left == 232 && logo.right == 296 && logo.top == 138 && logo.bottom == 222);
  assert(title.left == 141 && title.right == 387 && title.top == 270 && title.bottom == 311);
  assert(sleeping.left == 186 && sleeping.right == 340 && sleeping.top == 360 && sleeping.bottom == 380);
  assert(title.left + title.right == 528);
  assert(sleeping.left + sleeping.right == 526);
  assert(title.pixels > sleeping.pixels * 2);  // Bold title versus lighter status.

  // This exact raw/native landmark proves the portrait image used the confirmed
  // CCW transform, rather than being written in landscape or mirrored.
  assert(logicalBlack(frame, 141, 270));
  assert(nativeBlack(frame, 270, 386));

  assert(tamaink::sleep_screen::render(nullptr, kBytes, kNativeWidth, kNativeHeight, kStride) ==
         Status::InvalidArgument);
  expectRejectedWithoutWrite(Status::InvalidArgument, 528, 792, kStride, kBytes);
  expectRejectedWithoutWrite(Status::InvalidArgument, kNativeWidth, kNativeHeight, 98, kBytes);
  expectRejectedWithoutWrite(Status::InvalidArgument, kNativeWidth, kNativeHeight, kStride, kBytes - 1);
  expectRejectedWithoutWrite(Status::Overflow, kNativeWidth, kNativeHeight,
                             std::numeric_limits<std::size_t>::max(), kBytes);
  return 0;
}
