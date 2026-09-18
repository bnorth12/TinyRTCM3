#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

// =============================================================================
// CAP-SANITIZE — location sanitization (STUB for rewrite; drop path sketched)
// Requirements: REQ-SAN-01 (process), REQ-SAN-02 (API)
// Intent: prevent real ARP (1005/1006) and descriptive 1033 strings from entering
// public goldens. Preferred end-state: rewrite ARP to kPublishArpEcef01mm* via
// encode1005. Until encode exists, public path MUST drop location messages.
// =============================================================================

enum class SanitizeAction : uint8_t {
  Pass = 0,   // frame may be published as-is (non-location)
  Drop,       // omit from public corpus
  Rewrite     // replace body (Unsupported until Codec encode lands)
};

// Classify a complete frame for public export policy.
inline SanitizeAction sanitizeClassify(uint16_t messageType) {
  if (messageType == 1005 || messageType == 1006 || messageType == 1033) {
    return SanitizeAction::Drop;  // rewrite path not ready
  }
  return SanitizeAction::Pass;
}

// Attempt in-place rewrite of 1005/1006/1033 to published dummy identity.
// STUB: always Unsupported until CAP-CODEC-1005 encode + rewrite REQs are met.
Status sanitizeRewriteLocationFrame(uint8_t* frame, size_t len, size_t cap, size_t* outLen);

}  // namespace tinyrtcm3

