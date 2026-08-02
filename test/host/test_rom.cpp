#include "tamaink_rom.h"

#include <cassert>
#include <cstdint>
#include <vector>

struct Blob {
  std::vector<std::uint8_t> bytes;
  bool fail_size = false;
  bool fail_read = false;
  bool zero_after_first = false;
  bool overread = false;
  std::size_t size_override = static_cast<std::size_t>(-1);
  std::size_t calls = 0;
  std::size_t max_chunk = 0;
};

struct AllocationLog { std::size_t allocations = 0; std::size_t releases = 0; bool fail = false; };
static std::uint16_t* alloc_words(void* context, std::size_t count) {
  auto* log = static_cast<AllocationLog*>(context); ++log->allocations;
  return log->fail ? nullptr : new std::uint16_t[count];
}
static void free_words(void* context, std::uint16_t* words) {
  auto* log = static_cast<AllocationLog*>(context); ++log->releases; delete[] words;
}

static bool blob_size(void* context, std::size_t* size) {
  auto* blob = static_cast<Blob*>(context);
  if (blob->fail_size || size == nullptr) return false;
  *size = blob->size_override == static_cast<std::size_t>(-1) ? blob->bytes.size() : blob->size_override;
  return true;
}

static std::size_t blob_read(void* context, std::size_t offset,
                             std::uint8_t* destination, std::size_t capacity) {
  auto* blob = static_cast<Blob*>(context);
  ++blob->calls;
  if (blob->fail_read || offset >= blob->bytes.size()) return 0;
  if (blob->zero_after_first && blob->calls > 1) return 0;
  if (blob->overread) return capacity + 1;
  const std::size_t n = (blob->max_chunk != 0 && blob->max_chunk < capacity)
                            ? blob->max_chunk
                            : capacity;
  const std::size_t available = blob->bytes.size() - offset;
  const std::size_t count = n < available ? n : available;
  for (std::size_t i = 0; i < count; ++i) destination[i] = blob->bytes[offset + i];
  return count;
}

int main() {
  Blob blob;
  blob.bytes.resize(tamaink::rom::kPackedSize, 0);
  std::vector<std::uint16_t> instructions(tamaink::rom::kInstructionCount);
  tamaink::rom::Validation result{};
  tamaink::rom::ReadOnlySource source{&blob, blob_size, blob_read};

  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::Ok);
  assert(result.instruction_count == tamaink::rom::kInstructionCount);
  assert(result.crc32 == 0x70DEAA08u);
  assert(result.classification == tamaink::rom::Classification::StructurallyValidUnsupported);

  // Non-symmetric arithmetic fixture exercises every decoded word and CRC window.
  for (std::size_t i = 0; i < tamaink::rom::kInstructionCount; ++i) {
    const std::uint16_t word = static_cast<std::uint16_t>((i * 37u + 0xA5u) & 0x0FFFu);
    blob.bytes[i * 2] = static_cast<std::uint8_t>(word >> 8);
    blob.bytes[i * 2 + 1] = static_cast<std::uint8_t>(word);
  }
  for (std::size_t chunk : {std::size_t(1), std::size_t(3), std::size_t(255), std::size_t(256)}) {
    blob.max_chunk = chunk;
    assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::Ok);
    for (std::size_t i = 0; i < instructions.size(); ++i) {
      assert(instructions[i] == static_cast<std::uint16_t>((i * 37u + 0xA5u) & 0x0FFFu));
    }
    // Fixed independently from the arithmetic vector's decoded little-endian bytes.
    assert(result.crc32 == 0xFE68E27Bu);
  }
  AllocationLog log;
  tamaink::rom::Allocator allocator{&log, alloc_words, free_words};
  std::uint16_t* loaded = nullptr;
  blob.max_chunk = 3;
  assert(tamaink::rom::load(&source, &allocator, &loaded, &result) == tamaink::rom::Status::Ok);
  assert(loaded != nullptr && loaded[0] == 0xA5 && loaded[6143] == 0x880);
  allocator.release(allocator.context, loaded);
  assert(log.allocations == 1 && log.releases == 1);
  log.fail = true; loaded = nullptr;
  assert(tamaink::rom::load(&source, &allocator, &loaded, &result) == tamaink::rom::Status::AllocationFailure);
  assert(loaded == nullptr);
  log.fail = false; blob.fail_read = true;
  assert(tamaink::rom::load(&source, &allocator, &loaded, &result) == tamaink::rom::Status::ReadError);
  assert(log.releases == 2);
  blob.fail_read = false;

  // Decode known words at the beginning, CRC region, and end.
  blob.bytes.assign(tamaink::rom::kPackedSize, 0);
  auto pack = [&](std::size_t index, std::uint16_t value) {
    blob.bytes[index * 2] = static_cast<std::uint8_t>((value >> 8) & 0x0F);
    blob.bytes[index * 2 + 1] = static_cast<std::uint8_t>(value & 0xFF);
  };
  pack(0, 0x123);
  pack(0x2F0 / 2, 0x456);
  pack(0x2FE / 2, 0x789);
  pack(tamaink::rom::kInstructionCount - 1, 0xABC);
  blob.max_chunk = 256;
  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::Ok);
  assert(instructions[0] == 0x123 && instructions[0x2F0 / 2] == 0x456 && instructions[0x2FE / 2] == 0x789 && instructions.back() == 0xABC);

  // Every first byte is checked, including chunk boundaries.
  for (std::size_t bad : {std::size_t(0), std::size_t(2), std::size_t(12286)}) {
    blob.bytes.assign(tamaink::rom::kPackedSize, 0);
    blob.bytes[bad] = 0xF0;
    for (std::size_t chunk : {std::size_t(1), std::size_t(2), std::size_t(3), std::size_t(255), std::size_t(256)}) {
      blob.max_chunk = chunk;
      blob.calls = 0;
      assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::InvalidEncoding);
    }
  }

  blob.bytes.assign(tamaink::rom::kPackedSize, 0);
  blob.max_chunk = 3;
  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::Ok);
  blob.bytes[0] = 0xF0;
  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::InvalidEncoding);
  assert(result.crc32 == 0 && result.instruction_count == 0);
  blob.bytes[0] = 0;
  blob.fail_read = true;
  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::ReadError);
  assert(tamaink::rom::validate(nullptr, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::MissingSource);
  tamaink::rom::ReadOnlySource no_size{&blob, nullptr, blob_read};
  tamaink::rom::ReadOnlySource no_read{&blob, blob_size, nullptr};
  assert(tamaink::rom::validate(&no_size, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::MissingSource);
  assert(tamaink::rom::validate(&no_read, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::MissingSource);
  blob.fail_size = true;
  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::SizeFailure);
  blob.fail_size = false;
  assert(tamaink::rom::validate(&source, nullptr, instructions.size(), &result) == tamaink::rom::Status::NullOutput);
  assert(tamaink::rom::validate(&source, instructions.data(), 0, &result) == tamaink::rom::Status::OutputTooSmall);
  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size() - 1, &result) == tamaink::rom::Status::OutputTooSmall);
  blob.fail_read = false;
  blob.max_chunk = 256;
  blob.size_override = 0;
  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::SizeMismatch);
  blob.size_override = 1;
  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::SizeMismatch);
  blob.size_override = tamaink::rom::kPackedSize - 1;
  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::SizeMismatch);
  blob.size_override = tamaink::rom::kPackedSize + 1;
  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::SizeMismatch);
  blob.size_override = static_cast<std::size_t>(-1);
  blob.zero_after_first = true;
  blob.calls = 0;
  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::ReadError);
  blob.zero_after_first = false;
  blob.overread = true;
  blob.calls = 0;
  assert(tamaink::rom::validate(&source, instructions.data(), instructions.size(), &result) == tamaink::rom::Status::ReadError);
  assert(tamaink::rom::classify_crc32(tamaink::rom::kSupportedP1Crc32) == tamaink::rom::Classification::SupportedP1);
}
