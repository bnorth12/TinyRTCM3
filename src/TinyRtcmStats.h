#pragma once
#include <stdint.h>
#include "TinyRtcmTypes.h"
#include "TinyRtcmPolicy.h"

namespace tinyrtcm3 {

// CAP-STATS — stream counters + DF002 family histogram (REQ-STAT-01..03)

static constexpr size_t kTypeTableCap = 48;

// Fixed per-type row. Time fields stay zero until a non-zero nowMs is supplied.
struct TypeRow {
  uint16_t messageType = 0;
  uint32_t count = 0;
  uint32_t bytes = 0;
  uint32_t lastSeenMs = 0;
  uint32_t bytesPerSec = 0;
  uint32_t maxGapMs = 0;
  uint32_t windowBytes = 0;
  uint32_t windowStartMs = 0;
};

// Onocoy base observation set. Missing = never seen. Late = age > lateAfterMs.
static constexpr uint16_t kOnocoyRequiredTypes[] = {
    1005, 1033, 1077, 1087, 1097, 1117, 1127, 1137, 1230};
static constexpr size_t kOnocoyRequiredCount =
    sizeof(kOnocoyRequiredTypes) / sizeof(kOnocoyRequiredTypes[0]);
static constexpr uint32_t kDefaultLateAfterMs = 15000;

struct CoverageFlag {
  uint16_t messageType = 0;
  bool missing = true;
  bool late = false;
  bool required = true;
};

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
  // CRC-pass table, including frames the hub filter dropped. RAM only.
  TypeRow typeRows[kTypeTableCap] = {};
  uint8_t typeRowCount = 0;
  uint32_t typeRowOverflow = 0;
  // Runtime decode enablement. Default all off. Never persisted by this library.
  uint8_t decodeMask[512] = {};
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

inline TypeRow* statsFindType(StreamStats* s, uint16_t messageType, bool create) {
  if (s == nullptr || messageType == 0 || messageType > 4095) return nullptr;
  for (uint8_t i = 0; i < s->typeRowCount; ++i) {
    if (s->typeRows[i].messageType == messageType) return &s->typeRows[i];
  }
  if (!create) return nullptr;
  if (s->typeRowCount >= kTypeTableCap) {
    ++s->typeRowOverflow;
    return nullptr;
  }
  TypeRow& row = s->typeRows[s->typeRowCount++];
  row = TypeRow{};
  row.messageType = messageType;
  return &row;
}

inline const TypeRow* statsFindTypeConst(const StreamStats* s, uint16_t messageType) {
  if (s == nullptr || messageType == 0) return nullptr;
  for (uint8_t i = 0; i < s->typeRowCount; ++i) {
    if (s->typeRows[i].messageType == messageType) return &s->typeRows[i];
  }
  return nullptr;
}

// CRC-pass only. nowMs == 0 skips lastSeen / gap / bytes-per-second.
inline void statsOnCrcPass(StreamStats* s, uint16_t messageType, size_t frameLen, uint32_t nowMs) {
  if (s == nullptr || messageType == 0) return;
  TypeRow* row = statsFindType(s, messageType, true);
  if (row == nullptr) return;
  ++row->count;
  row->bytes += static_cast<uint32_t>(frameLen);
  if (nowMs == 0) return;
  if (row->lastSeenMs != 0 && nowMs >= row->lastSeenMs) {
    const uint32_t gap = nowMs - row->lastSeenMs;
    if (gap > row->maxGapMs) row->maxGapMs = gap;
  }
  row->lastSeenMs = nowMs;
  if (row->windowStartMs == 0) row->windowStartMs = nowMs;
  row->windowBytes += static_cast<uint32_t>(frameLen);
  const uint32_t elapsed = nowMs - row->windowStartMs;
  if (elapsed >= 1000u) {
    row->bytesPerSec =
        static_cast<uint32_t>((static_cast<uint64_t>(row->windowBytes) * 1000ull) / elapsed);
    row->windowBytes = 0;
    row->windowStartMs = nowMs;
  }
}

inline void decodeMaskSet(StreamStats* s, uint16_t messageType, bool enabled) {
  if (s == nullptr || messageType > 4095) return;
  const uint16_t byte = static_cast<uint16_t>(messageType >> 3);
  const uint8_t bit = static_cast<uint8_t>(1u << (messageType & 7));
  if (enabled) s->decodeMask[byte] = static_cast<uint8_t>(s->decodeMask[byte] | bit);
  else s->decodeMask[byte] = static_cast<uint8_t>(s->decodeMask[byte] & static_cast<uint8_t>(~bit));
}

inline bool decodeMaskGet(const StreamStats* s, uint16_t messageType) {
  if (s == nullptr || messageType > 4095) return false;
  const uint8_t bit = static_cast<uint8_t>(1u << (messageType & 7));
  return (s->decodeMask[messageType >> 3] & bit) != 0;
}

inline void decodeMaskClear(StreamStats* s) {
  if (s == nullptr) return;
  for (size_t i = 0; i < sizeof(s->decodeMask); ++i) s->decodeMask[i] = 0;
}

// Writes kOnocoyRequiredCount flags (or cap, whichever is smaller).
// nowMs == 0 forces late = false. Missing means count == 0.
inline void coverageCheck(const StreamStats* s, uint32_t nowMs, uint32_t lateAfterMs,
                          CoverageFlag* out, size_t cap) {
  if (out == nullptr) return;
  const size_t n = cap < kOnocoyRequiredCount ? cap : kOnocoyRequiredCount;
  for (size_t i = 0; i < n; ++i) {
    out[i].messageType = kOnocoyRequiredTypes[i];
    out[i].required = true;
    out[i].missing = true;
    out[i].late = false;
    const TypeRow* row = statsFindTypeConst(s, kOnocoyRequiredTypes[i]);
    if (row == nullptr || row->count == 0) continue;
    out[i].missing = false;
    if (nowMs != 0 && row->lastSeenMs != 0 && lateAfterMs > 0 && nowMs > row->lastSeenMs &&
        (nowMs - row->lastSeenMs) > lateAfterMs) {
      out[i].late = true;
    }
  }
}

}  // namespace tinyrtcm3
