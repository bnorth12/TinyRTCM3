#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

struct Msg1005 {
  uint16_t stationId = 0;
  int64_t ecefX01mm = 0;
  int64_t ecefY01mm = 0;
  int64_t ecefZ01mm = 0;
};

struct Msg1033 {
  uint16_t stationId = 0;
  // Antenna/receiver descriptors — keep short; real parse TBD.
  char antennaDescriptor[32] = {};
  char receiverDescriptor[32] = {};
};

struct MsmHeaderCnrSummary {
  uint16_t messageType = 0;  // 1074..1127
  uint16_t stationId = 0;
  uint8_t satCount = 0;
  uint8_t sigCount = 0;
  // Rough quality: mean CNR (0.1 dB-Hz) over present cells; 0xFFFF = N/A
  uint16_t meanCnr01dBHz = 0xFFFF;
};

// v0.1: declarations only — implementations land with synthetic golden tests.
Status decode1005(const uint8_t* frame, size_t len, Msg1005* out);
Status encode1005(const Msg1005& msg, uint8_t* out, size_t cap, size_t* outLen);
Status encode1033(const Msg1033& msg, uint8_t* out, size_t cap, size_t* outLen);
Status summarizeMsmCnr(const uint8_t* frame, size_t len, MsmHeaderCnrSummary* out);

}  // namespace tinyrtcm3
