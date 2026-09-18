#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

// CAP-SANITIZE ? classify/drop/rewrite location messages for public export.
// REQ-SAN-01 (process), REQ-SAN-02 (API). 1005 rewrite uses publish dummy ARP;
// 1006/1033 still Drop until their encode paths land.

enum class SanitizeAction : uint8_t {
  Pass = 0,
  Drop,
  Rewrite
};

inline SanitizeAction sanitizeClassify(uint16_t messageType) {
  if (messageType == 1005) return SanitizeAction::Rewrite;
  if (messageType == 1006 || messageType == 1033) return SanitizeAction::Drop;
  return SanitizeAction::Pass;
}

Status sanitizeRewriteLocationFrame(uint8_t* frame, size_t len, size_t cap, size_t* outLen);

}  // namespace tinyrtcm3
