#pragma once
#include <stdint.h>
#include <stddef.h>

namespace tinyrtcm3 {

// Library version (keep in sync with library.properties / library.json).
static constexpr uint16_t kVersionMajor = 0;
static constexpr uint16_t kVersionMinor = 1;
static constexpr uint16_t kVersionPatch = 0;

// Compile-time capability flags — flip when implementations land.
static constexpr bool kCapCrc24qCompute = true;
static constexpr bool kCapCrc24qVerify = true;
static constexpr bool kCapCrc24qAppend = true;
static constexpr bool kCapFrameAssembler = true;
static constexpr bool kCapBitBufferWrite = true;
static constexpr bool kCapHubPassthrough = true;
static constexpr bool kCapDecode1005 = false;        // Codec stub
static constexpr bool kCapEncode1005 = false;        // Codec stub
static constexpr bool kCapEncode1033 = false;        // Codec stub
static constexpr bool kCapMsmCnrSummary = false;     // Codec stub
static constexpr bool kCapCrc24qTableAccel = false;  // bit-at-a-time only
static constexpr bool kCapFieldGoldenCi = false;     // CI = synthetic only

struct Requirement {
  const char* id;     // stable REQ-ID for traces / CI reports
  const char* title;
  bool implemented;
  const char* note;   // nullptr or short limitation
};

// Authoritative v0.1 matrix. Verify asserts implemented entries; stubs expected Unsupported.
static constexpr Requirement kRequirements[] = {
    {"REQ-CRC-01", "CRC-24Q compute over transport body", true, nullptr},
    {"REQ-CRC-02", "CRC-24Q verify complete frames", true, nullptr},
    {"REQ-CRC-03", "CRC-24Q append / finalizeFrame for encode", true,
     "bit-at-a-time; no table accel yet"},
    {"REQ-ASM-01", "Byte-stream frame assembler with BadCrc", true, nullptr},
    {"REQ-BIT-01", "MSB-first bit writer (BitBuffer)", true, "write-only stub depth"},
    {"REQ-HUB-01", "Passthrough hub filter+emit", true, nullptr},
    {"REQ-COD-01", "Decode RTCM 1005", false, "returns Unsupported"},
    {"REQ-COD-02", "Encode RTCM 1005", false, "returns Unsupported"},
    {"REQ-COD-03", "Encode RTCM 1033", false, "returns Unsupported"},
    {"REQ-COD-04", "MSM header + CNR summary", false, "returns Unsupported"},
    {"REQ-GOLD-01", "Synthetic goldens are CI contract", true, nullptr},
    {"REQ-PRIV-01", "No unsanitized field ARP in public tree", true,
     "sanitizer drops 1005/1006/1033 until rewrite encoder"},
    {"REQ-VER-01", "In-library runSelfTests for implemented caps", true, nullptr},
};

static constexpr size_t kRequirementCount =
    sizeof(kRequirements) / sizeof(kRequirements[0]);

inline const Requirement* findRequirement(const char* id) {
  if (id == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < kRequirementCount; ++i) {
    const char* a = kRequirements[i].id;
    const char* b = id;
    while (*a != 0 && *b != 0 && *a == *b) {
      ++a;
      ++b;
    }
    if (*a == 0 && *b == 0) {
      return &kRequirements[i];
    }
  }
  return nullptr;
}

}  // namespace tinyrtcm3
