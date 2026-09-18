#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

// =============================================================================
// Codec — selective RTCM message body decode/encode
// Capabilities: CAP-CODEC-1005 / 1006 / 1033 / MSM (see TinyRtcmRequirements.h)
// Requirements: REQ-COD-1005-*, REQ-COD-1006-*, REQ-COD-1033-*, REQ-COD-MSM-*
// Intent: expose a stable API surface for station ARP, descriptors, and MSM
// quality summaries. v0.1 implements decode1005; encode/rewrite/1006/1033/MSM
// remain Unsupported until later milestones.
// Non-goal (v1): full MSM observation cell encode/decode (REQ-COD-MSM-X).
// =============================================================================

// RTCM 1005 — Stationary ARP (ECEF). Units: 0.0001 m (0.1 mm).
struct Msg1005 {
  uint16_t stationId = 0;
  int64_t ecefX01mm = 0;
  int64_t ecefY01mm = 0;
  int64_t ecefZ01mm = 0;
};

// RTCM 1006 — ARP + antenna height above marker (0.0001 m).
struct Msg1006 {
  uint16_t stationId = 0;
  int64_t ecefX01mm = 0;
  int64_t ecefY01mm = 0;
  int64_t ecefZ01mm = 0;
  int32_t antennaHeight01mm = 0;
};

// RTCM 1033 — Antenna / receiver descriptors (short fixed buffers for MCU use).
struct Msg1033 {
  uint16_t stationId = 0;
  char antennaDescriptor[32] = {};
  char receiverDescriptor[32] = {};
};

// MSM4/7 header glance + mean CNR — NOT full observations.
struct MsmHeaderCnrSummary {
  uint16_t messageType = 0;  // 1074..1127
  uint16_t stationId = 0;
  uint8_t satCount = 0;
  uint8_t sigCount = 0;
  uint16_t meanCnr01dBHz = 0xFFFF;  // 0xFFFF = N/A
};

// --- 1005 (CAP-CODEC-1005) ---------------------------------------------------
Status decode1005(const uint8_t* frame, size_t len, Msg1005* out);
Status encode1005(const Msg1005& msg, uint8_t* out, size_t cap, size_t* outLen);
// Privacy rewrite: station id + ARP -> kPublishStationId / kPublishArpEcef01mm*.
Status rewrite1005ToPublishIdentity(const uint8_t* in, size_t inLen, uint8_t* out,
                                    size_t cap, size_t* outLen);

// --- 1006 (CAP-CODEC-1006) ---------------------------------------------------
Status decode1006(const uint8_t* frame, size_t len, Msg1006* out);
Status encode1006(const Msg1006& msg, uint8_t* out, size_t cap, size_t* outLen);

// --- 1033 (CAP-CODEC-1033) ---------------------------------------------------
Status decode1033(const uint8_t* frame, size_t len, Msg1033* out);
Status encode1033(const Msg1033& msg, uint8_t* out, size_t cap, size_t* outLen);

// --- MSM summary (CAP-CODEC-MSM) ---------------------------------------------
Status summarizeMsmCnr(const uint8_t* frame, size_t len, MsmHeaderCnrSummary* out);

}  // namespace tinyrtcm3
