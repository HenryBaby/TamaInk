#include "tamaink_persistence.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace tamaink::persist;

namespace {
const std::uint8_t kRom[kRomIdentitySize] = {1, 2, 3, 4, 5, 6, 7, 8};

Record makeRecord(std::uint32_t length) {
  Record record{};
  std::memcpy(record.rom, kRom, kRomIdentitySize);
  record.generation = 0x10203040u;
  record.timestamp = 0x0102030405060708ull;
  record.payloadLength = length;
  for (std::uint32_t i = 0; i < length; ++i) record.payload[i] = static_cast<std::uint8_t>(i);
  return record;
}

void assertRoundTrip(const Record& input) {
  std::uint8_t bytes[kHeaderSize + kMaxPayload]{};
  const std::size_t size = encode(input, bytes, sizeof bytes);
  assert(size == kHeaderSize + input.payloadLength);
  Record output{};
  assert(decode(bytes, size, kRom, output) == DecodeError::None);
  assert(std::memcmp(&output, &input, sizeof input) == 0);
}

int bootSelection(const std::uint8_t* a, std::size_t aSize,
                  const std::uint8_t* b, std::size_t bSize) {
  Record records[2]{};
  auto valid = [](const std::uint8_t* bytes, std::size_t size, Record& record) {
    if (!bytes || decode(bytes, size, kRom, record) != DecodeError::None || record.payloadLength != 8) return false;
    for (std::uint8_t i = 0; i < 8; ++i) {
      if (record.payload[i] != static_cast<std::uint8_t>(record.generation + i)) return false;
    }
    return true;
  };
  const bool validA = valid(a, aSize, records[0]);
  const bool validB = valid(b, bSize, records[1]);
  return selectNewest(&records[0], validA, &records[1], validB);
}
}

int main() {
  Record record = makeRecord(5);
  std::uint8_t bytes[kHeaderSize + kMaxPayload]{};
  assert(encode(record, nullptr, sizeof bytes) == 0);
  assert(encode(record, bytes, kHeaderSize + record.payloadLength - 1) == 0);
  record.payloadLength = kMaxPayload + 1;
  assert(encode(record, bytes, sizeof bytes) == 0);
  record = makeRecord(5);

  const std::size_t size = encode(record, bytes, sizeof bytes);
  assert(size == 45);
  const std::uint8_t expectedHeader[] = {
      0x54, 0x49, 0x4e, 0x4b, 0x01, 0x00, 0x28, 0x00,
      0x4b, 0x4e, 0x49, 0x54, 1, 2, 3, 4, 5, 6, 7, 8,
      0x40, 0x30, 0x20, 0x10, 0x05, 0x00, 0x00, 0x00,
      0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
  assert(std::memcmp(bytes, expectedHeader, sizeof expectedHeader) == 0);
  assertRoundTrip(makeRecord(0));
  assertRoundTrip(makeRecord(kMaxPayload));

  Record output{};
  Record oldRecord = makeRecord(8);
  oldRecord.generation = 10;
  Record newRecord = makeRecord(8);
  newRecord.generation = 11;
  for (std::uint8_t i = 0; i < 8; ++i) {
    oldRecord.payload[i] = static_cast<std::uint8_t>(oldRecord.generation + i);
    newRecord.payload[i] = static_cast<std::uint8_t>(newRecord.generation + i);
  }
  std::uint8_t oldBytes[kHeaderSize + kMaxPayload]{};
  std::uint8_t committedBytes[kHeaderSize + kMaxPayload]{};
  const std::size_t oldSize = encode(oldRecord, oldBytes, sizeof oldBytes);
  const std::size_t newSize = encode(newRecord, committedBytes, sizeof committedBytes);
  std::uint8_t stagedBytes[kHeaderSize + kMaxPayload]{};
  std::memcpy(stagedBytes, committedBytes, newSize);
  assert(stageEncodedRecord(stagedBytes, newSize));
  assert(verifyStagedRecord(stagedBytes, newSize, committedBytes, newSize));
  assert(decode(stagedBytes, newSize, kRom, output) == DecodeError::BadCrc);

  // Resets at Open, Partial, CompleteUnsynced, Synced, and Verified must keep
  // the old committed slot selected. Only the final CRC write commits the new slot.
  assert(bootSelection(oldBytes, oldSize, nullptr, 0) == 0);
  assert(bootSelection(oldBytes, oldSize, stagedBytes, kHeaderSize / 2) == 0);
  assert(bootSelection(oldBytes, oldSize, stagedBytes, newSize) == 0);
  assert(bootSelection(oldBytes, oldSize, stagedBytes, newSize) == 0);
  assert(bootSelection(oldBytes, oldSize, stagedBytes, newSize) == 0);
  assert(commitStagedRecord(stagedBytes, newSize, committedBytes, newSize));
  assert(bootSelection(oldBytes, oldSize, stagedBytes, newSize) == 1);
  stagedBytes[kHeaderSize] ^= 1;
  assert(bootSelection(oldBytes, oldSize, stagedBytes, newSize) == 0);
  assert(!verifyStagedRecord(stagedBytes, newSize, committedBytes, newSize));

  assert(decode(nullptr, 0, kRom, output) == DecodeError::Truncated);
  assert(decode(bytes, 0, kRom, output) == DecodeError::Truncated);
  assert(decode(bytes, size, nullptr, output) == DecodeError::None);

  auto expectError = [&](std::size_t count, DecodeError error, auto mutate) {
    std::uint8_t copy[sizeof bytes]{};
    std::memcpy(copy, bytes, size);
    mutate(copy);
    assert(decode(copy, count, kRom, output) == error);
  };
  expectError(kHeaderSize - 1, DecodeError::Truncated, [](std::uint8_t*) {});
  expectError(size, DecodeError::BadMagic, [](std::uint8_t* p) { p[0] ^= 1; });
  expectError(size, DecodeError::BadVersion, [](std::uint8_t* p) { p[4] = 2; });
  expectError(size, DecodeError::BadHeader, [](std::uint8_t* p) { p[5] = 1; });
  expectError(size, DecodeError::BadHeader, [](std::uint8_t* p) { p[6] = 0; });
  expectError(size, DecodeError::BadCompatibility, [](std::uint8_t* p) { p[8] ^= 1; });
  expectError(size, DecodeError::BadRom, [](std::uint8_t* p) { p[12] ^= 1; });
  expectError(size, DecodeError::BadLength, [](std::uint8_t* p) {
    p[24] = 0xff;
    p[25] = 0xff;
    p[26] = 0xff;
    p[27] = 0xff;
  });
  expectError(size - 1, DecodeError::Truncated, [](std::uint8_t*) {});
  expectError(size + 1, DecodeError::Trailing, [](std::uint8_t* p) { p[size] = 0; });
  expectError(size, DecodeError::BadCrc, [](std::uint8_t* p) { p[kHeaderSize] ^= 1; });

  assert(generationNewer(1, 0));
  assert(generationNewer(0, 0xffffffffu));
  assert(!generationNewer(5, 5));
  assert(!generationNewer(0x80000000u, 0));
  assert(!generationNewer(0, 0x80000000u));

  Record a = record;
  Record b = record;
  a.generation = 7;
  b.generation = 6;
  assert(selectNewest(&a, true, &b, true) == 0);
  assert(selectNewest(&b, true, &a, true) == 1);
  a.generation = b.generation;
  assert(selectNewest(&a, true, &b, true) == 0);
  assert(selectNewest(&a, true, nullptr, true) == 0);
  assert(selectNewest(nullptr, true, &b, true) == 1);
  assert(selectNewest(&a, false, &b, true) == 1);
  assert(selectNewest(&a, true, &b, false) == 0);
  assert(selectNewest(&a, false, &b, false) == -1);

  std::cout << "persistence tests passed\n";
}
