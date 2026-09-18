#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

// CAP-POLICY — stock Hub filter predicates
// REQ-POL-01/02, REQ-POL-03 (ISO-only)

inline bool policyIsStationIdentityType(uint16_t messageType) {
  return messageType == 1005 || messageType == 1006 || messageType == 1033;
}

inline bool policyIsMsmType(uint16_t messageType) {
  return messageType >= 1071 && messageType <= 1127;
}

inline bool policyIsMsm4Type(uint16_t messageType) {
  return policyIsMsmType(messageType) && (messageType % 10) == 4;
}

inline bool policyIsMsm7Type(uint16_t messageType) {
  return policyIsMsmType(messageType) && (messageType % 10) == 7;
}

// Keep station identity + MSM (rover-lite / base mission diet).
inline bool policyFilterKeepStationAndMsm(uint16_t messageType, const uint8_t* /*frame*/,
                                          size_t /*len*/, void* /*ctx*/) {
  return policyIsStationIdentityType(messageType) || policyIsMsmType(messageType);
}

// Drop MSM; pass everything else.
inline bool policyFilterDropMsm(uint16_t messageType, const uint8_t* /*frame*/, size_t /*len*/,
                                void* /*ctx*/) {
  return !policyIsMsmType(messageType);
}

// ISO-only: 1005 / 1006 / 1033 (REQ-POL-03).
inline bool policyFilterIsoOnly(uint16_t messageType, const uint8_t* /*frame*/, size_t /*len*/,
                                void* /*ctx*/) {
  return policyIsStationIdentityType(messageType);
}

}  // namespace tinyrtcm3
