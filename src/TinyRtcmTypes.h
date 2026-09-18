#pragma once
#include <stdint.h>
#include <stddef.h>

namespace tinyrtcm3 {

// ---------------------------------------------------------------------------
// Shared result codes for the entire library.
// Intent: callers can switch on a single Status for transport, codec, and hub
// paths without mapping vendor-specific error enums. Prefer returning Status
// over asserting; embedded RTK pipelines must keep streaming on soft failures.
// ---------------------------------------------------------------------------
enum class Status : uint8_t {
  Ok = 0,          // Operation completed; for feed() a full CRC-valid frame is ready
  NeedMore,        // Stream consumer needs additional bytes (not an error)
  BadCrc,          // Frame candidate failed CRC-24Q; assembler discarded it
  Overflow,        // Output buffer / bit buffer capacity exceeded
  Unsupported,     // Declared API exists but this build has no implementation yet
  InvalidArg       // Null pointer, length mismatch, or otherwise illegal call
};

// Non-owning view of one complete RTCM 3 transport frame (0xD3 .. CRC).
// Intent: zero-copy handoff from FrameAssembler / Hub to app or NTRIP code.
struct FrameView {
  const uint8_t* data = nullptr;  // full frame including preamble and CRC
  size_t length = 0;
  uint16_t messageType = 0;       // DF002-style 12-bit type extracted from payload
};

// Published dummy Antenna Reference Point for sanitized public field goldens.
// Intent: never ship real survey ECEF in the public repo. Sanitizer / encode1005
// rewrite paths MUST target these constants (see docs/PRIVACY.md and ICD §Privacy).
// Units: 0.0001 m (0.1 mm), ECEF. Approx WGS84 lat 0°, lon 0°, h ≈ 0.
static constexpr int64_t kPublishArpEcef01mmX = 63781370000LL;
static constexpr int64_t kPublishArpEcef01mmY = 0LL;
static constexpr int64_t kPublishArpEcef01mmZ = 0LL;
static constexpr uint16_t kPublishStationId = 0;

}  // namespace tinyrtcm3
