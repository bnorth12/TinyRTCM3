#pragma once
#include <stdint.h>
#include <stddef.h>

namespace tinyrtcm3 {

// CRC-24Q (poly 0x1864CFB), RTCM 3 transport.
inline uint32_t crc24q(const uint8_t* data, size_t len) {
  uint32_t crc = 0;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint32_t>(data[i]) << 16;
    for (int b = 0; b < 8; ++b) {
      crc <<= 1;
      if (crc & 0x1000000u) {
        crc ^= 0x1864CFBu;
      }
    }
  }
  return crc & 0xFFFFFFu;
}

inline bool frameCrcOk(const uint8_t* frame, size_t len) {
  if (frame == nullptr || len < 6) return false;
  const uint32_t got =
      (static_cast<uint32_t>(frame[len - 3]) << 16) |
      (static_cast<uint32_t>(frame[len - 2]) << 8) |
      static_cast<uint32_t>(frame[len - 1]);
  return crc24q(frame, len - 3) == got;
}

inline uint16_t messageType(const uint8_t* frame, size_t len) {
  if (frame == nullptr || len < 5) return 0;
  return static_cast<uint16_t>((static_cast<uint16_t>(frame[3]) << 4) |
                               (static_cast<uint16_t>(frame[4]) >> 4));
}

}  // namespace tinyrtcm3
