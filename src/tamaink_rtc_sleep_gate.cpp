#include "tamaink_rtc_sleep_gate.h"
namespace tamaink::rtc {
namespace {
bool bcd(std::uint8_t v) { return (v & 0x0f) <= 9 && (v >> 4) <= 9; }
bool leap(unsigned y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
unsigned dim(unsigned y, unsigned m) {
  static const unsigned d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return m == 2 ? d[m - 1] + leap(y) : d[m - 1];
}
} // namespace
bool decodeDs3231(const std::uint8_t r[7], DateTime &o, std::uint64_t &epoch) {
  if (!r)
    return false;
  const bool h12 = (r[2] & 0x40) != 0;
  const std::uint8_t sr = r[0] & 0x7f, mr = r[1] & 0x7f,
                     hr = r[2] & (h12 ? 0x1f : 0x3f), wr = r[3],
                     dr = r[4] & 0x3f, monr = r[5] & 0x1f, yr = r[6];
  if (!bcd(sr) || !bcd(mr) || !bcd(hr) || !bcd(wr) || !bcd(dr) || !bcd(monr) ||
      !bcd(yr) || (r[0] & 0x80) || (r[1] & 0x80) || (r[2] & 0x80) ||
      (r[3] & 0xf8) || (r[4] & 0xc0) || (r[5] & 0x60))
    return false;
  unsigned sec = (sr >> 4) * 10 + (sr & 15), min = (mr >> 4) * 10 + (mr & 15),
           hour = (hr >> 4) * 10 + (hr & 15), wd = (wr >> 4) * 10 + (wr & 15),
           day = (dr >> 4) * 10 + (dr & 15),
           mon = (monr >> 4) * 10 + (monr & 15),
           year = 2000 + ((r[5] & 0x80) ? 100 : 0) + (yr >> 4) * 10 + (yr & 15);
  if (sec > 59 || min > 59 || wd < 1 || wd > 7 || mon < 1 || mon > 12 ||
      day < 1 || day > dim(year, mon) || year > 2199)
    return false;
  if (h12) {
    if (hour < 1 || hour > 12)
      return false;
    if (r[2] & 0x20)
      hour += (hour == 12 ? 0 : 12);
    else if (hour == 12)
      hour = 0;
  } else if (hour > 23)
    return false;
  std::uint64_t days = 0;
  for (unsigned y = 2000; y < year; ++y)
    days += leap(y) ? 366 : 365;
  for (unsigned m = 1; m < mon; ++m)
    days += dim(year, m);
  days += day - 1;
  epoch = days * 86400ULL + hour * 3600ULL + min * 60ULL + sec;
  o = {static_cast<std::uint16_t>(year), static_cast<std::uint8_t>(mon),
       static_cast<std::uint8_t>(day),   static_cast<std::uint8_t>(hour),
       static_cast<std::uint8_t>(min),   static_cast<std::uint8_t>(sec),
       static_cast<std::uint8_t>(wd)};
  return true;
}
std::uint64_t encodeTagged(std::uint64_t e) {
  return e <= kMaxEpochSeconds ? kTag | e : 0;
}
bool decodeTagged(std::uint64_t v, std::uint64_t &e) {
  if ((v & kTagMask) != kTag)
    return false;
  e = v & ~kTagMask;
  return e <= kMaxEpochSeconds;
}
bool elapsed(std::uint64_t saved, std::uint64_t current,
             std::uint64_t &seconds) {
  if (current < saved)
    return false;
  seconds = current - saved;
  return true;
}
} // namespace tamaink::rtc
