#include "tamaink_rom.h"

#include <cstddef>
#include <cstdint>

namespace tamaink::rom {
namespace {

void set_failure(Validation* result, Status status) {
  if (result != nullptr) {
    result->status = status;
    result->classification = Classification::StructurallyValidUnsupported;
    result->crc32 = 0;
    result->instruction_count = 0;
  }
}

void crc_byte(std::uint32_t* crc, std::uint8_t byte) {
  *crc ^= byte;
  for (unsigned bit = 0; bit < 8; ++bit) {
    *crc = (*crc & 1u) != 0u ? (*crc >> 1u) ^ 0xEDB88320u : *crc >> 1u;
  }
}

}  // namespace

Classification classify_crc32(std::uint32_t crc32) {
  return crc32 == kSupportedP1Crc32 ? Classification::SupportedP1
                                    : Classification::StructurallyValidUnsupported;
}

Status validate(const ReadOnlySource* source, std::uint16_t* instructions,
                std::size_t instruction_capacity, Validation* result) {
  set_failure(result, Status::MissingSource);
  if (source == nullptr || source->size == nullptr || source->read == nullptr) {
    return Status::MissingSource;
  }
  if (instructions == nullptr) {
    set_failure(result, Status::NullOutput);
    return Status::NullOutput;
  }
  if (instruction_capacity < kInstructionCount) {
    set_failure(result, Status::OutputTooSmall);
    return Status::OutputTooSmall;
  }

  std::size_t size = 0;
  if (!source->size(source->context, &size)) {
    set_failure(result, Status::SizeFailure);
    return Status::SizeFailure;
  }
  if (size != kPackedSize) {
    set_failure(result, Status::SizeMismatch);
    return Status::SizeMismatch;
  }

  std::uint8_t chunk[256];
  std::size_t offset = 0;
  std::size_t instruction_index = 0;
  std::uint8_t first = 0;
  bool have_first = false;
  std::uint32_t crc = 0xFFFFFFFFu;

  while (offset < kPackedSize) {
    const std::size_t requested = (kPackedSize - offset < sizeof(chunk))
                                      ? kPackedSize - offset
                                      : sizeof(chunk);
    const std::size_t got = source->read(source->context, offset, chunk, requested);
    if (got == 0 || got > requested) {
      set_failure(result, Status::ReadError);
      return Status::ReadError;
    }
    for (std::size_t i = 0; i < got; ++i) {
      const std::uint8_t byte = chunk[i];
      if (!have_first) {
        if ((byte & 0xF0u) != 0u) {
          set_failure(result, Status::InvalidEncoding);
          return Status::InvalidEncoding;
        }
        first = byte;
        have_first = true;
      } else {
        const std::uint16_t instruction =
            static_cast<std::uint16_t>((first & 0x0Fu) << 8u) |
            static_cast<std::uint16_t>(byte);
        instructions[instruction_index++] = instruction;
        const std::size_t decoded_offset = (instruction_index - 1u) * 2u;
        if (decoded_offset >= 0x2F0u && decoded_offset < 0x400u) {
          crc_byte(&crc, static_cast<std::uint8_t>(instruction & 0xFFu));
          crc_byte(&crc, static_cast<std::uint8_t>((instruction >> 8u) & 0xFFu));
        }
        have_first = false;
      }
    }
    offset += got;
  }
  if (have_first || instruction_index != kInstructionCount) {
    set_failure(result, Status::ReadError);
    return Status::ReadError;
  }

  const std::uint32_t final_crc = ~crc;
  if (result != nullptr) {
    result->status = Status::Ok;
    result->classification = classify_crc32(final_crc);
    result->crc32 = final_crc;
    result->instruction_count = instruction_index;
  }
  return Status::Ok;
}

Status load(const ReadOnlySource* source, const Allocator* allocator,
            std::uint16_t** instructions, Validation* result) {
  set_failure(result, Status::AllocationFailure);
  if (instructions == nullptr) return Status::AllocationFailure;
  *instructions = nullptr;
  if (allocator == nullptr || allocator->allocate == nullptr || allocator->release == nullptr) {
    return Status::AllocationFailure;
  }
  std::uint16_t* allocated = allocator->allocate(allocator->context, kInstructionCount);
  if (allocated == nullptr) return Status::AllocationFailure;
  const Status status = validate(source, allocated, kInstructionCount, result);
  if (status != Status::Ok) {
    allocator->release(allocator->context, allocated);
    return status;
  }
  *instructions = allocated;
  return Status::Ok;
}

}  // namespace tamaink::rom
