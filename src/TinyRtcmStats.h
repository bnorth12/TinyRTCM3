#pragma once
#include <stdint.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

// =============================================================================
// CAP-STATS — stream counters (STUB)
// Requirements: REQ-STAT-01, REQ-STAT-02
// Intent: field bring-up metrics (CRC health, filter drops, throughput) without
// pulling in a logging framework. Hub integration TBD; apps may update manually.
// =============================================================================

struct StreamStats {
  uint32_t bytesIn = 0;
  uint32_t framesOk = 0;
  uint32_t framesBadCrc = 0;
  uint32_t framesDroppedByFilter = 0;
  uint32_t framesOverflow = 0;
};

inline void statsReset(StreamStats* s) {
  if (s == nullptr) return;
  *s = StreamStats{};
}

inline void statsOnByte(StreamStats* s) {
  if (s) ++s->bytesIn;
}

inline void statsOnStatus(StreamStats* s, Status st, bool droppedByFilter = false) {
  if (s == nullptr) return;
  if (droppedByFilter) {
    ++s->framesDroppedByFilter;
    return;
  }
  switch (st) {
    case Status::Ok:
      ++s->framesOk;
      break;
    case Status::BadCrc:
      ++s->framesBadCrc;
      break;
    case Status::Overflow:
      ++s->framesOverflow;
      break;
    default:
      break;
  }
}

}  // namespace tinyrtcm3

