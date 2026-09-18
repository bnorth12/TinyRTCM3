#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

// =============================================================================
// CAP-BIT-WRITE / CAP-BIT-READ — MSB-first bit buffer for RTCM data fields
// Requirements: REQ-BIT-01 (write, implemented), REQ-BIT-02 (read, stub)
// Intent: RTCM message bodies are packed MSB-first across byte boundaries.
// Codec encode uses putBits; decode needs getBits (not yet implemented).
// =============================================================================
class BitBuffer {
 public:
  BitBuffer(uint8_t* data, size_t capBytes) : data_(data), cap_(capBytes), bitPos_(0) {}

  // Clear buffer and write cursor (REQ-BIT-01 setup).
  void resetWrite() {
    bitPos_ = 0;
    if (data_ != nullptr && cap_ != 0) {
      memset(data_, 0, cap_);
    }
  }

  // Position read cursor at bit 0 without clearing bytes (for decode).
  void resetRead() { bitPos_ = 0; }

  // Append nbits (1..32) MSB-first from value into data_ (REQ-BIT-01).
  Status putBits(uint32_t value, uint8_t nbits) {
    if (nbits == 0 || nbits > 32) return Status::InvalidArg;
    if (data_ == nullptr) return Status::InvalidArg;
    for (int i = static_cast<int>(nbits) - 1; i >= 0; --i) {
      const size_t byteIndex = bitPos_ / 8;
      if (byteIndex >= cap_) return Status::Overflow;
      const uint8_t bit = static_cast<uint8_t>((value >> i) & 1u);
      data_[byteIndex] |= static_cast<uint8_t>(bit << (7 - (bitPos_ % 8)));
      ++bitPos_;
    }
    return Status::Ok;
  }

  // Read nbits MSB-first into *out (REQ-BIT-02). STUB: always Unsupported.
  Status getBits(uint8_t nbits, uint32_t* out) {
    (void)nbits;
    (void)out;
    // Intent: mirror putBits bit order; required before real decode1005/etc.
    return Status::Unsupported;
  }

  size_t byteLength() const { return (bitPos_ + 7) / 8; }
  size_t bitLength() const { return bitPos_; }

 private:
  uint8_t* data_;
  size_t cap_;
  size_t bitPos_;
};

}  // namespace tinyrtcm3
