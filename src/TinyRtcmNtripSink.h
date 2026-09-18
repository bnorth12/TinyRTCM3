#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

// =============================================================================
// CAP-NTRIP-BOUNDARY — application owns NTRIP/TCP/TLS (documentation stub)
// Requirements: REQ-NTRIP-01
// Intent: TinyRTCM3 stops at framed RTCM bytes. Casters, GGA GGA-ntrip, auth,
// and reconnect policy belong in the firmware app (or a separate module).
// This interface exists so Hub Emit can be pointed at an app-provided sink
// without the library growing socket code.
// =============================================================================

class INtripSink {
 public:
  virtual ~INtripSink() = default;
  // Send one complete RTCM frame (includes CRC). Returns Ok or implementation-defined error.
  virtual Status writeFrame(const uint8_t* frame, size_t len) = 0;
};

// Adapter: Hub Emit callback that forwards to INtripSink* stored in ctx.
inline void ntripSinkEmit(const uint8_t* frame, size_t len, void* ctx) {
  if (ctx == nullptr || frame == nullptr) return;
  static_cast<INtripSink*>(ctx)->writeFrame(frame, len);
}

}  // namespace tinyrtcm3

