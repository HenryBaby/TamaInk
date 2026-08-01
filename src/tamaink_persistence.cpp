#include "tamaink_persistence.h"
#include <cstring>

namespace tamaink::persist {
namespace {
void put16(std::uint8_t* p, std::uint16_t v) {
  p[0] = static_cast<std::uint8_t>(v);
  p[1] = static_cast<std::uint8_t>(v >> 8);
}
void put32(std::uint8_t* p, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
void put64(std::uint8_t* p, std::uint64_t v) {
  for (int i = 0; i < 8; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
std::uint16_t get16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
std::uint32_t get32(const std::uint8_t* p) {
  std::uint32_t v = 0;
  for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(p[i]) << (8 * i);
  return v;
}
std::uint64_t get64(const std::uint8_t* p) {
  std::uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(p[i]) << (8 * i);
  return v;
}
std::uint32_t recordCrc(const std::uint8_t* p, std::size_t len) {
  std::uint32_t crc = 0xFFFFFFFFu;
  auto update = [&crc](const std::uint8_t* bytes, std::size_t count) {
    while (count-- != 0) {
      crc ^= *bytes++;
      for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
  };
  update(p, 36);
  update(p + kHeaderSize, len);
  return ~crc;
}
}

std::size_t encode(const Record& r, std::uint8_t* out, std::size_t cap) {
  if (!out || r.payloadLength > kMaxPayload || cap < kHeaderSize + r.payloadLength) return 0;
  put32(out, kMagic);
  out[4] = kFormatVersion;
  out[5] = 0;
  put16(out + 6, kHeaderSize);
  put32(out + 8, kCompatibilityId);
  std::memcpy(out + 12, r.rom, kRomIdentitySize);
  put32(out + 20, r.generation);
  put32(out + 24, r.payloadLength);
  put64(out + 28, r.timestamp);
  std::memcpy(out + kHeaderSize, r.payload, r.payloadLength);
  put32(out + 36, recordCrc(out, r.payloadLength));
  return kHeaderSize + r.payloadLength;
}

DecodeError decode(const std::uint8_t* d, std::size_t n, const std::uint8_t expectedRom[kRomIdentitySize], Record& out) {
  if (!d || n < kHeaderSize) return DecodeError::Truncated;
  if (get32(d) != kMagic) return DecodeError::BadMagic;
  if (d[4] != kFormatVersion) return DecodeError::BadVersion;
  if (d[5] != 0 || get16(d + 6) != kHeaderSize) return DecodeError::BadHeader;
  if (get32(d + 8) != kCompatibilityId) return DecodeError::BadCompatibility;
  if (expectedRom && std::memcmp(d + 12, expectedRom, kRomIdentitySize) != 0) return DecodeError::BadRom;
  const std::uint32_t len = get32(d + 24);
  if (len > kMaxPayload) return DecodeError::BadLength;
  if (n < kHeaderSize + len) return DecodeError::Truncated;
  if (n > kHeaderSize + len) return DecodeError::Trailing;
  if (get32(d + 36) != recordCrc(d, len)) return DecodeError::BadCrc;
  std::memcpy(out.rom, d + 12, kRomIdentitySize);
  out.generation = get32(d + 20);
  out.payloadLength = len;
  out.timestamp = get64(d + 28);
  std::memcpy(out.payload, d + kHeaderSize, len);
  return DecodeError::None;
}

bool stageEncodedRecord(std::uint8_t* data, std::size_t size) {
  if (!data || size < kHeaderSize) return false;
  data[kCrcOffset] ^= 0x01u;
  return true;
}

bool verifyStagedRecord(const std::uint8_t* staged, std::size_t stagedSize,
                        const std::uint8_t* committed, std::size_t committedSize) {
  if (!staged || !committed || stagedSize != committedSize || stagedSize < kHeaderSize) return false;
  bool crcDiffers = false;
  for (std::size_t i = 0; i < stagedSize; ++i) {
    if (i >= kCrcOffset && i < kCrcOffset + kCrcSize) {
      crcDiffers = crcDiffers || staged[i] != committed[i];
    } else if (staged[i] != committed[i]) {
      return false;
    }
  }
  return crcDiffers;
}

bool commitStagedRecord(std::uint8_t* staged, std::size_t stagedSize,
                        const std::uint8_t* committed, std::size_t committedSize) {
  if (!verifyStagedRecord(staged, stagedSize, committed, committedSize)) return false;
  std::memcpy(staged + kCrcOffset, committed + kCrcOffset, kCrcSize);
  return true;
}

bool generationNewer(std::uint32_t a, std::uint32_t b) {
  const std::uint32_t diff = a - b;
  return diff != 0u && diff < 0x80000000u;
}
int selectNewest(const Record* a, bool av, const Record* b, bool bv) {
  av = av && a != nullptr; bv = bv && b != nullptr;
  if (av && bv) {
    if (generationNewer(a->generation, b->generation)) return 0;
    if (generationNewer(b->generation, a->generation)) return 1;
    return 0;
  }
  if (av) return 0;
  if (bv) return 1;
  return -1;
}
const char* decodeErrorName(DecodeError e) {
  switch (e) {
    case DecodeError::None: return "valid";
    case DecodeError::Truncated: return "truncated";
    case DecodeError::Trailing: return "trailing";
    case DecodeError::BadMagic: return "bad-magic";
    case DecodeError::BadVersion: return "bad-version";
    case DecodeError::BadHeader: return "bad-header";
    case DecodeError::BadCompatibility: return "bad-compat";
    case DecodeError::BadRom: return "bad-rom";
    case DecodeError::BadLength: return "bad-length";
    case DecodeError::BadCrc: return "bad-crc";
  }
  return "unknown";
}
}
