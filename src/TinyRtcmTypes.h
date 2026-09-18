#pragma once
#include <stdint.h>
#include <stddef.h>

namespace tinyrtcm3 {

enum class Status : uint8_t {
  Ok = 0,
  NeedMore,
  BadCrc,
  Overflow,
  Unsupported,
  InvalidArg
};

struct FrameView {
  const uint8_t* data = nullptr; // full frame incl. 0xD3..CRC
  size_t length = 0;
  uint16_t messageType = 0;
};

// Published dummy ARP for sanitized public field goldens (NOT a real site).
// ECEF meters ≈ lat 0°, lon 0°, h = 0 (WGS84). Documented in docs/PRIVACY.md.
static constexpr int64_t kPublishArpEcef01mmX = 63781370000LL; // 0.0001 m units
static constexpr int64_t kPublishArpEcef01mmY = 0LL;
static constexpr int64_t kPublishArpEcef01mmZ = 0LL;
static constexpr uint16_t kPublishStationId = 0;

}  // namespace tinyrtcm3
