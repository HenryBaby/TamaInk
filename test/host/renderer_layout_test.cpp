#include "tamaink_renderer_layout.h"
#include <cassert>

int main() {
  constexpr auto x3 = tamaink::render::layoutForGeometry(792, 528);
  static_assert(x3.lcdScale == 16 && x3.lcdOriginX == 268 && x3.lcdOriginY == 8);
  assert(x3.iconScale == 16);
  constexpr auto x4 = tamaink::render::layoutForGeometry(800, 480);
  static_assert(x4.lcdScale == 14 && x4.lcdOriginX == 288 && x4.lcdOriginY == 16);
  assert(x4.iconScale == 16);
  return 0;
}
