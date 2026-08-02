#pragma once
#include <cstddef>
#include <cstdint>

namespace tamaink::sleep_screen {
enum class Status : std::uint8_t { Ok, InvalidArgument, Overflow };

// Draws the terminal sleep screen into a caller-owned native 1bpp framebuffer.
// Native geometry is 792x528 (logical portrait 528x792, CCW mapped).
Status render(std::uint8_t* destination, std::size_t capacity,
              std::uint16_t width, std::uint16_t height, std::size_t stride);
}
