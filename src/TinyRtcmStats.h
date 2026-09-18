#pragma once
#include <stdint.h>
#include "TinyRtcmTypes.h"
#include "TinyRtcmPolicy.h"

namespace tinyrtcm3 {

// CAP-STATS — stream counters + DF002 family histogram (REQ-STAT-01..03)

struct StreamStats {
  uint32_t bytesIn = 0;
  uint32_t framesOk = 0;
  uint32_t framesBadCrc = 0;
  uint32_t framesDroppedByFilter = 0;
  uint32_t framesOverflow = 0;
  // Type histogram (OK frames that passed the filter / were counted as Ok).
  uint32_t type1005 = 0;
  uint32_t type1006 = 0;
  uint32_t type1033 = 0;
  uint32_t typeMsm4 = 0;
  uint32_t typeMsm7 = 0;
  uint32_t typeOther = 0;
};

inline void statsReset(StreamStats* s) {
  if (s == nullptr) return;
  *s = StreamStats{};
}

inline void statsOnByte(StreamStats* s) {
  if (s) ++s->bytesIn;
}

inline void statsOnFrameType(StreamStats* s, uint16_t messageType) {
  if (s == nullptr) return;
  if (messageType == 1005) ++s->type1005;
  else if (messageType == 1006) ++s->type1006;
  else if (messageType == 1033) ++s->type1033;
  else if (policyIsMsm4Type(messageType)) ++s->typeMsm4;
  else if (policyIsMsm7Type(messageType)) ++s->typeMsm7;
  else ++s->typeOther;
}

inline void statsOnStatus(StreamStats* s, Status st, bool droppedByFilter = false,
                          uint16_t messageType = 0) {
  if (s == nullptr) return;
  if (droppedByFilter) {
    ++s->framesDroppedByFilter;
    return;
  }
  switch (st) {
    case Status::Ok:
      ++s->framesOk;
      if (messageType != 0) statsOnFrameType(s, messageType);
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
