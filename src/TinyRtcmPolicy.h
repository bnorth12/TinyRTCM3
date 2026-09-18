#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

// =============================================================================
// CAP-POLICY â€” stock Hub filter predicates (STUB)
// Requirements: REQ-POL-01, REQ-POL-02 (see TinyRtcmRequirements.h / docs/ICD.md)
// Intent: give applications copy-paste-safe filters (ISO station msgs only,
// drop MSM, allowlist) so every firmware does not invent its own type ranges.
// // =============================================================================

// True if type is in common "station identity" set: 1005, 1006, 1033.
// Stub: implements the predicate logic (no I/O); safe to use once Hub is wired.
inline bool policyIsStationIdentityType(uint16_t messageType) {
  return messageType == 1005 || messageType == 1006 || messageType == 1033;
}

// True if type looks like MSM4/MSM7 (1074..1127 typical bands).
inline bool policyIsMsmType(uint16_t messageType) {
  return messageType >= 1071 && messageType <= 1127;
}

// Hub-compatible filter: keep station identity + MSM; drop everything else.
inline bool policyFilterKeepStationAndMsm(uint16_t messageType, const uint8_t* /*frame*/,
                                          size_t /*len*/, void* /*ctx*/) {
  return policyIsStationIdentityType(messageType) || policyIsMsmType(messageType);
}

// Hub-compatible filter: drop MSM (pass non-MSM).
inline bool policyFilterDropMsm(uint16_t messageType, const uint8_t* /*frame*/, size_t /*len*/,
                                void* /*ctx*/) {
  return !policyIsMsmType(messageType);
}

}  // namespace tinyrtcm3

