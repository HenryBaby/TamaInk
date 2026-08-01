#pragma once

#include <cstddef>
#include <cstdint>
#include "tamaink_tamalib.h"

namespace tamaink::render {

enum class Status : std::uint8_t { Ok, InvalidArgument, Overflow };
enum class Rotation : std::uint8_t { None, CounterClockwise90 };

// Renders the 32x16 Tama LCD into a caller-owned 1bpp, row-major,
// MSB-first destination. The destination is cleared white; icon pixels are
// intentionally omitted until their placement semantics are proven. With
// CounterClockwise90, the logical footprint is 16x32 cells and (col,row)
// maps to (row,31-col) in y-down physical coordinates.
Status snapshot(const tamalib::Snapshot& source, std::uint8_t* destination,
               std::size_t capacity, std::uint16_t width, std::uint16_t height,
               std::size_t stride, std::int32_t originX, std::int32_t originY,
               std::uint16_t scale, Rotation rotation = Rotation::None);

} // namespace tamaink::render
