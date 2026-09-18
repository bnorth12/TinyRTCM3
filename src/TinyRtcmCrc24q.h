#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

// CRC-24Q for RTCM 3 transport (Qualcomm poly 0x1864CFB, init 0).
// Covers preamble + length + payload; stored as 3 MSB-first bytes after payload.
// Every complete RTCM 3 frame must have a valid CRC; assemblers verify, encoders
// must append via appendCrc24q / finalizeFrame before emit.
static constexpr uint32_t kCrc24qPoly = 0x1864CFBu;
static constexpr size_t kRtcmMinFrameLen = 6;  // preamble+len+CRC, zero payload
static constexpr size_t kRtcmCrcLen = 3;

inline uint32_t crc24q(const uint8_t* data, size_t len) {
  uint32_t crc = 0;
  if (data == nullptr && len != 0) return 0;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint32_t>(data[i]) << 16;
    for (int b = 0; b < 8; ++b) {
      crc <<= 1;
      if (crc & 0x1000000u) {
        crc ^= kCrc24qPoly;
      }
    }
  }
  return crc & 0xFFFFFFu;
}

inline void storeCrc24q(uint8_t out[3], uint32_t crc) {
  out[0] = static_cast<uint8_t>((crc >> 16) & 0xFF);
  out[1] = static_cast<uint8_t>((crc >> 8) & 0xFF);
  out[2] = static_cast<uint8_t>(crc & 0xFF);
}

inline uint32_t loadCrc24q(const uint8_t in[3]) {
  return (static_cast<uint32_t>(in[0]) << 16) |
         (static_cast<uint32_t>(in[1]) << 8) |
         static_cast<uint32_t>(in[2]);
}

// Verify CRC of a complete frame (0xD3 .. CRC). False if too short or mismatch.
inline bool frameCrcOk(const uint8_t* frame, size_t len) {
  if (frame == nullptr || len < kRtcmMinFrameLen) return false;
  const uint32_t got = loadCrc24q(frame + len - kRtcmCrcLen);
  return crc24q(frame, len - kRtcmCrcLen) == got;
}

// Append CRC-24Q after preamble+length+payload already in frame[0..bodyLen).
// bodyLen must be 3 + payloadLen. Writes 3 bytes; *outLen = bodyLen + 3.
inline Status appendCrc24q(uint8_t* frame, size_t bodyLen, size_t cap, size_t* outLen) {
  if (frame == nullptr || outLen == nullptr) return Status::InvalidArg;
  if (bodyLen < 3) return Status::InvalidArg;
  if (cap < bodyLen + kRtcmCrcLen) return Status::Overflow;
  if (frame[0] != 0xD3) return Status::InvalidArg;
  const size_t payload =
      (static_cast<size_t>(frame[1] & 0x03u) << 8) | static_cast<size_t>(frame[2]);
  if (bodyLen != payload + 3u) return Status::InvalidArg;
  const uint32_t c = crc24q(frame, bodyLen);
  storeCrc24q(frame + bodyLen, c);
  *outLen = bodyLen + kRtcmCrcLen;
  return Status::Ok;
}

// Build transport framing around payload: 0xD3 | 10-bit length | payload | CRC.
inline Status finalizeFrame(const uint8_t* payload, size_t payloadLen, uint8_t* out,
                            size_t cap, size_t* outLen) {
  if (out == nullptr || outLen == nullptr) return Status::InvalidArg;
  if (payloadLen > 1023u) return Status::Overflow;
  if (payload == nullptr && payloadLen != 0) return Status::InvalidArg;
  const size_t need = payloadLen + 6u;
  if (cap < need) return Status::Overflow;
  out[0] = 0xD3;
  out[1] = static_cast<uint8_t>((payloadLen >> 8) & 0x03u);
  out[2] = static_cast<uint8_t>(payloadLen & 0xFFu);
  if (payloadLen) {
    for (size_t i = 0; i < payloadLen; ++i) out[3 + i] = payload[i];
  }
  return appendCrc24q(out, payloadLen + 3u, cap, outLen);
}

inline uint16_t messageType(const uint8_t* frame, size_t len) {
  if (frame == nullptr || len < 5) return 0;
  return static_cast<uint16_t>((static_cast<uint16_t>(frame[3]) << 4) |
                               (static_cast<uint16_t>(frame[4]) >> 4));
}

}  // namespace tinyrtcm3
