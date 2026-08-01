#pragma once

#include <cstddef>
#include <cstdint>

namespace tamaink::rom {

constexpr std::size_t kPackedSize = 12288;
constexpr std::size_t kInstructionCount = 6144;
constexpr std::uint32_t kSupportedP1Crc32 = 0xC7875F27u;

using SizeReader = bool (*)(void* context, std::size_t* size);
using ByteReader = std::size_t (*)(void* context, std::size_t offset,
                                   std::uint8_t* destination,
                                   std::size_t capacity);

struct ReadOnlySource {
  void* context;
  SizeReader size;
  ByteReader read;
};

enum class Status : std::uint8_t {
  Ok = 0,
  MissingSource,
  SizeFailure,
  SizeMismatch,
  ReadError,
  AllocationFailure,
  NullOutput,
  OutputTooSmall,
  InvalidEncoding,
};

using AllocateInstructions = std::uint16_t* (*)(void* context, std::size_t count);
using FreeInstructions = void (*)(void* context, std::uint16_t* instructions);
struct Allocator {
  void* context;
  AllocateInstructions allocate;
  FreeInstructions release;
};

enum class Classification : std::uint8_t {
  SupportedP1 = 0,
  StructurallyValidUnsupported,
};

struct Validation {
  Status status;
  Classification classification;
  std::uint32_t crc32;
  std::size_t instruction_count;
};

// Reads and validates a packed P1 ROM without retaining the packed bytes.
// The destination receives decoded 12-bit instructions in host-independent
// numeric form (the value itself, not a byte-swapped representation). Output
// contents are unspecified unless Status::Ok is returned.
Status validate(const ReadOnlySource* source, std::uint16_t* instructions,
                std::size_t instruction_capacity, Validation* result);

Classification classify_crc32(std::uint32_t crc32);

// Allocates a decoded instruction array through the caller-provided allocator.
// On success, *instructions is owned by the caller and must be released with
// allocator.release. It remains null on failure; allocation is bounded.
Status load(const ReadOnlySource* source, const Allocator* allocator,
            std::uint16_t** instructions, Validation* result);

}  // namespace tamaink::rom
