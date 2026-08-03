#pragma once

#include <cstddef>
#include <cstdint>
#include "tamaink_tamalib.h"
#include "tamaink_settings.h"

namespace tamaink::render {

enum class Status : std::uint8_t { Ok, InvalidArgument, Overflow };
enum class Rotation : std::uint8_t { None, CounterClockwise90 };
enum class IconLayout : std::uint8_t { None, P1BottomRow };

struct BatteryStatus {
  bool percentageKnown = false;
  std::uint8_t percentage = 0;
  bool visible = true;
};

// Renders the 32x16 Tama LCD into a caller-owned 1bpp, row-major,
// MSB-first destination. The destination is cleared white; icon pixels are
// omitted by default, or rendered using an explicit P1 icon layout. With
// CounterClockwise90, the logical footprint is 16x32 cells and (col,row)
// maps to (row,31-col) in y-down physical coordinates.
Status snapshot(const tamalib::Snapshot& source, std::uint8_t* destination,
               std::size_t capacity, std::uint16_t width, std::uint16_t height,
               std::size_t stride, std::int32_t originX, std::int32_t originY,
               std::uint16_t scale, Rotation rotation = Rotation::None,
               IconLayout iconLayout = IconLayout::None,
               BatteryStatus battery = {}, std::uint16_t iconScale = 0);

// Composites a centered settings popup into an already-rendered framebuffer.
Status overlaySettings(std::uint8_t* destination, std::size_t capacity,
                       std::uint16_t width, std::uint16_t height,
                       std::size_t stride, const settings::Values& values,
                       std::uint8_t focus, bool confirmation = false,
                       bool resetYes = false);

} // namespace tamaink::render
