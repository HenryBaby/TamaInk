#pragma once

#include <cstddef>
#include <cstdint>

namespace tamaink::persist {

constexpr std::uint32_t kMagic = 0x4B4E4954u; // "TINK" little endian
constexpr std::uint8_t kFormatVersion = 1;
constexpr std::uint32_t kCompatibilityId = 0x54494E4Bu;
constexpr std::size_t kRomIdentitySize = 8;
constexpr std::size_t kHeaderSize = 40;
constexpr std::size_t kMaxPayload = 256;
constexpr std::size_t kCrcOffset = 36;
constexpr std::size_t kCrcSize = 4;

enum class DecodeError : std::uint8_t {
  None,
  Truncated,
  Trailing,
  BadMagic,
  BadVersion,
  BadHeader,
  BadCompatibility,
  BadRom,
  BadLength,
  BadCrc,
};

struct Record {
  std::uint8_t rom[kRomIdentitySize]{};
  std::uint32_t generation = 0;
  std::uint64_t timestamp = 0;
  std::uint8_t payload[kMaxPayload]{};
  std::uint32_t payloadLength = 0;
};

std::size_t encode(const Record& record, std::uint8_t* out, std::size_t capacity);
DecodeError decode(const std::uint8_t* data, std::size_t size, const std::uint8_t expectedRom[kRomIdentitySize], Record& out);
bool stageEncodedRecord(std::uint8_t* data, std::size_t size);
bool verifyStagedRecord(const std::uint8_t* staged, std::size_t stagedSize,
                        const std::uint8_t* committed, std::size_t committedSize);
bool commitStagedRecord(std::uint8_t* staged, std::size_t stagedSize,
                        const std::uint8_t* committed, std::size_t committedSize);
bool generationNewer(std::uint32_t a, std::uint32_t b);
// Returns 0/1 for the newest valid slot; equal or ambiguous generations choose slot 0.
int selectNewest(const Record* a, bool aValid, const Record* b, bool bValid);
const char* decodeErrorName(DecodeError error);

} // namespace tamaink::persist
