#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

// CAP-SANITIZE — classify/drop/rewrite location messages for public export.
// REQ-SAN-01 (process), REQ-SAN-02 (API). 1005/1006 rewrite to publish dummy ARP;
// 1033 rewrite uses sanitized short descriptors.

enum class SanitizeAction : uint8_t {
  Pass = 0,
  Drop,
  Rewrite
};

inline SanitizeAction sanitizeClassify(uint16_t messageType) {
  if (messageType == 1005 || messageType == 1006 || messageType == 1033)
    return SanitizeAction::Rewrite;
  return SanitizeAction::Pass;
}

Status sanitizeRewriteLocationFrame(uint8_t* frame, size_t len, size_t cap, size_t* outLen);

}  // namespace tinyrtcm3
