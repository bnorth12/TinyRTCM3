#pragma once
#include <stdint.h>
#include <stddef.h>

namespace tinyrtcm3 {

// =============================================================================
// Requirements & capabilities (machine-readable companion to docs/ICD.md)
// =============================================================================
// Intent:
//   - Every *capability* the library claims (or plans) is a Capability entry.
//   - Every capability lists one or more REQ-IDs with shall-statements.
//   - `implemented` on the capability must match reality of this build.
//   - Flip capability + REQ rows in the SAME commit as the implementation.
//   - Humans read docs/ICD.md for full ICD prose; this header is the checklist
//     CI / runSelfTests / reviewers use.
// =============================================================================

static constexpr uint16_t kVersionMajor = 0;
static constexpr uint16_t kVersionMinor = 5;
static constexpr uint16_t kVersionPatch = 0;

// --- Individual requirement (shall-statement) --------------------------------
struct Requirement {
  const char* id;     // stable forever, e.g. "REQ-CRC-01"
  const char* shall;  // normative shall-statement (one sentence)
  bool met;           // true iff this build satisfies the shall
  const char* note;   // limitation / deferred work (nullable)
};

// --- Capability groups one or more requirements under a product feature ------
struct Capability {
  const char* id;              // e.g. "CAP-CRC-24Q"
  const char* name;            // short human title
  const char* intent;          // why this capability exists in the stack
  bool implemented;            // true iff all *required-for-v0.1* REQs below are met
  const char* const* reqIds;   // nullptr-terminated list of REQ-ID strings
  const char* scopeNote;       // v1 scope / non-goals (nullable)
};

// -----------------------------------------------------------------------------
// Requirements catalog (normative shalls)
// -----------------------------------------------------------------------------
static constexpr Requirement kRequirements[] = {
    // Transport CRC
    {"REQ-CRC-01",
     "The library shall compute CRC-24Q over RTCM3 preamble+length+payload.", true,
     nullptr},
    {"REQ-CRC-02",
     "The library shall verify CRC-24Q on complete frames and report failure.", true,
     nullptr},
    {"REQ-CRC-03",
     "The library shall append CRC-24Q when finalizing an outbound frame.", true,
     "evaluate+generate foundational; table is CAP-CRC-TABLE"},
    {"REQ-CRC-04",
     "The library shall offer optional table-accelerated CRC-24Q.", true,
     "table default; bit path for identity tests"},

    // Assembler
    {"REQ-ASM-01",
     "The library shall assemble a byte stream into CRC-valid RTCM3 frames.", true,
     nullptr},
    {"REQ-ASM-02",
     "The library shall return BadCrc and resync when a candidate CRC fails.", true,
     nullptr},
    {"REQ-ASM-03",
     "The library shall reject frames that exceed the internal buffer capacity.", true,
     nullptr},

    // Bit buffer
    {"REQ-BIT-01",
     "The library shall write MSB-first bit fields into a caller buffer.", true,
     nullptr},
    {"REQ-BIT-02",
     "The library shall read MSB-first bit fields from a caller buffer.", true,
     nullptr},

    // Hub
    {"REQ-HUB-01",
     "The library shall provide a hub that assembles, optionally filters, and emits frames.",
     true, nullptr},
    {"REQ-HUB-02",
     "The hub shall drop a frame when the filter callback returns false without treating it as error.",
     true, nullptr},

    // Policy helpers
    {"REQ-POL-01",
     "The library shall provide stock filter helpers (e.g. ISO-only, drop-MSM).", true, nullptr},
    {"REQ-POL-02",
     "Stock filters shall be pure predicates usable with Hub::setFilter.", true, nullptr},

    // Registry
    {"REQ-REG-01",
     "The library shall provide a message-type registry for dispatch by DF002 type.", true, nullptr},
    {"REQ-REG-02",
     "Unregistered types shall be reportable as Unsupported without aborting the stream.", true, nullptr},

    // Stats
    {"REQ-STAT-01",
     "The library shall count frames OK, BadCrc, filter drops, and bytes in.", true, nullptr},
    {"REQ-STAT-02",
     "Stream stats shall be resettable without affecting assembler state.", true, nullptr},

    // Codec 1005/1006/1033/MSM
    {"REQ-COD-1005-D", "The library shall decode RTCM 1005 ARP fields into Msg1005.", true,
     nullptr},
    {"REQ-COD-1005-E", "The library shall encode Msg1005 into a CRC-valid 1005 frame.", true, nullptr},
    {"REQ-COD-1006-D", "The library shall decode RTCM 1006 (ARP + antenna height).", true,
     "implemented"},
    {"REQ-COD-1006-E", "The library shall encode Msg1006 from ECEF + antenna height.", true,
     "implemented"},
    {"REQ-COD-1033-D", "The library shall decode RTCM 1033 descriptors.", true,
     "implemented"},
    {"REQ-COD-1033-E", "The library shall encode Msg1033 into a CRC-valid 1033 frame.", true,
     "implemented"},
    {"REQ-COD-1005-R",
     "The library shall rewrite 1005 station id and ARP to published dummy constants.", true, nullptr},
    {"REQ-COD-MSM-S",
     "The library shall summarize MSM4/7 headers and mean CNR without full obs cells.", true,
     "implemented"},
    {"REQ-COD-MSM-X",
     "The library shall NOT encode MSM or 1230 in v1 (non-goal).", true,
     "met by refusing encode; returns Unsupported"},

    // Privacy / sanitize
    {"REQ-SAN-01",
     "Public field goldens shall never contain unsanitized real ARP or 1033 strings.", true,
     "process + gitignore + sanitizer script"},
    {"REQ-SAN-02",
     "C++ sanitize API shall drop or rewrite location messages before public export.", true, "1005/1006 rewrite; 1033 sanitized; sanitizeBeforeEmit"},

    // Goldens / verify
    {"REQ-GOLD-01", "CI shall treat synthetic goldens as the merge contract.", true, nullptr},
    {"REQ-GOLD-02", "Field goldens shall be optional soak and sanitizer-gated.", true, nullptr},
    {"REQ-VER-01", "The library shall expose runSelfTests covering implemented capabilities.",
     true, nullptr},
    {"REQ-VER-02", "Self-test shall treat unimplemented Codec APIs as expected Unsupported skips.",
     true, nullptr},


    // Solid-library extensions (v0.5)
    {"REQ-POL-03", "The library shall provide an ISO-only stock filter (1005/1006/1033).", true,
     "policyFilterIsoOnly"},
    {"REQ-HUB-03", "The library shall accept a byte buffer via Hub::feed(data,len).", true,
     "batch feed"},
    {"REQ-STAT-03", "Stream stats shall count OK frames by type family (1005/1006/1033/MSM4/MSM7/other).",
     true, "histogram"},
    {"REQ-COD-1006-R", "The library shall rewrite 1006 station id and ARP to published dummy constants.",
     true, "rewrite1006ToPublishIdentity"},
    {"REQ-SAN-03", "The library shall provide a sanitize-before-emit helper for Hub Emit adapters.",
     true, "sanitizeBeforeEmit"},
    {"REQ-VER-03", "Self-test shall prove table CRC bit-identical to the bit engine.", true, nullptr},

    // NTRIP (out of library proper)
    {"REQ-NTRIP-01",
     "NTRIP/TCP/TLS shall remain application-owned; library may expose only a sink stub.",
     true, "INtripSink stub documents the boundary"},
};

static constexpr size_t kRequirementCount =
    sizeof(kRequirements) / sizeof(kRequirements[0]);

inline const Requirement* findRequirement(const char* id) {
  if (id == nullptr) return nullptr;
  for (size_t i = 0; i < kRequirementCount; ++i) {
    const char* a = kRequirements[i].id;
    const char* b = id;
    while (*a && *b && *a == *b) {
      ++a;
      ++b;
    }
    if (*a == 0 && *b == 0) return &kRequirements[i];
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
// Capability -> requirement ID tables (nullptr-terminated)
// -----------------------------------------------------------------------------
static constexpr const char* kReqIds_Crc24q[] = {
    "REQ-CRC-01", "REQ-CRC-02", "REQ-CRC-03", nullptr};
static constexpr const char* kReqIds_CrcTable[] = {"REQ-CRC-04", nullptr};
static constexpr const char* kReqIds_Assembler[] = {
    "REQ-ASM-01", "REQ-ASM-02", "REQ-ASM-03", nullptr};
static constexpr const char* kReqIds_BitWrite[] = {"REQ-BIT-01", nullptr};
static constexpr const char* kReqIds_BitRead[] = {"REQ-BIT-02", nullptr};
static constexpr const char* kReqIds_Hub[] = {"REQ-HUB-01", "REQ-HUB-02", "REQ-HUB-03", nullptr};
static constexpr const char* kReqIds_Policy[] = {"REQ-POL-01", "REQ-POL-02", "REQ-POL-03", nullptr};
static constexpr const char* kReqIds_Registry[] = {"REQ-REG-01", "REQ-REG-02", nullptr};
static constexpr const char* kReqIds_Stats[] = {"REQ-STAT-01", "REQ-STAT-02", "REQ-STAT-03", nullptr};
static constexpr const char* kReqIds_Codec1005[] = {
    "REQ-COD-1005-D", "REQ-COD-1005-E", "REQ-COD-1005-R", nullptr};
static constexpr const char* kReqIds_Codec1006[] = {
    "REQ-COD-1006-D", "REQ-COD-1006-E", "REQ-COD-1006-R", nullptr};
static constexpr const char* kReqIds_Codec1033[] = {
    "REQ-COD-1033-D", "REQ-COD-1033-E", nullptr};
static constexpr const char* kReqIds_CodecMsm[] = {
    "REQ-COD-MSM-S", "REQ-COD-MSM-X", nullptr};
static constexpr const char* kReqIds_Sanitize[] = {"REQ-SAN-01", "REQ-SAN-02", "REQ-SAN-03", nullptr};
static constexpr const char* kReqIds_Goldens[] = {"REQ-GOLD-01", "REQ-GOLD-02", nullptr};
static constexpr const char* kReqIds_SelfTest[] = {"REQ-VER-01", "REQ-VER-02", "REQ-VER-03", nullptr};
static constexpr const char* kReqIds_NtripBoundary[] = {"REQ-NTRIP-01", nullptr};

// -----------------------------------------------------------------------------
// Capabilities catalog (each documents its requirements via reqIds)
// -----------------------------------------------------------------------------
static constexpr Capability kCapabilities[] = {
    {"CAP-CRC-24Q", "CRC-24Q transport",
     "Guarantee every RTCM3 frame on the wire is integrity-checked or correctly sealed.",
     true, kReqIds_Crc24q, "evaluate+generate; table default"},
    {"CAP-CRC-TABLE", "CRC-24Q table accel",
     "Optional speed path for high-rate UART without changing CRC results.", true,
     kReqIds_CrcTable, "table default; TINYRTCM3_CRC_BIT_ONLY for bit path"},
    {"CAP-FRAME-ASM", "Frame assembler",
     "Turn UART/NTRIP byte streams into discrete CRC-valid frames for policy/codec.", true,
     kReqIds_Assembler, nullptr},
    {"CAP-BIT-WRITE", "Bit buffer write",
     "Pack RTCM DF bit fields MSB-first when encoding message bodies.", true, kReqIds_BitWrite,
     nullptr},
    {"CAP-BIT-READ", "Bit buffer read",
     "Unpack RTCM DF bit fields MSB-first when decoding message bodies.", true, kReqIds_BitRead,
     nullptr},
    {"CAP-HUB", "Passthrough hub",
     "App-facing pipe: assemble -> optional filter -> emit toward radio/NTRIP/log.", true,
     kReqIds_Hub, nullptr},
    {"CAP-POLICY", "Stock policy filters",
     "Reusable predicates (ISO-only, drop MSM, allowlist) so apps do not reinvent filters.",
     true, kReqIds_Policy, nullptr},
    {"CAP-REGISTRY", "Message registry",
     "Map DF002 message type to decode/summarize handlers without switch soup in apps.", true,
     kReqIds_Registry, nullptr},
    {"CAP-STATS", "Stream statistics",
     "Lightweight counters for field bring-up and soak (CRC fails, drops, throughput).", true,
     kReqIds_Stats, nullptr},
    {"CAP-CODEC-1005", "RTCM 1005 codec",
     "Station ARP decode/encode and privacy rewrite for public goldens / NTRIP identity.", true, kReqIds_Codec1005, "decode/encode/rewrite 1005 met"},
    {"CAP-CODEC-1006", "RTCM 1006 codec",
     "ARP + antenna height; synthetic-first (LC29H typically cannot TX).", true,
     kReqIds_Codec1006, "encode/decode + synthetic golden"},
    {"CAP-CODEC-1033", "RTCM 1033 codec",
     "Antenna/receiver descriptors; encode for honest station metadata, sanitize for publish.",
     true, kReqIds_Codec1033, "encode/decode + sanitized synthetic golden"},
    {"CAP-CODEC-MSM", "MSM header+CNR summary",
     "Quality glance at MSM4/7 without storing full observation cells (v1 non-goal: encode MSM).",
     true, kReqIds_CodecMsm, "MSM4/7 header+mean CNR; encode MSM out of scope"},
    {"CAP-SANITIZE", "Location sanitization",
     "Keep real farm/shop ECEF and 1033 strings out of public artifacts.", true, kReqIds_Sanitize,
     "1005/1006 rewrite; 1033 sanitized; sanitizeBeforeEmit"},
    {"CAP-GOLDENS", "Golden corpora",
     "Synthetic CI contract + optional sanitized field soak.", true, kReqIds_Goldens, nullptr},
    {"CAP-SELFTEST", "In-library self-test",
     "Exercise implemented capabilities during development and in CI.", true, kReqIds_SelfTest,
     nullptr},
    {"CAP-NTRIP-BOUNDARY", "NTRIP boundary",
     "Document that sockets/TLS/NTRIP are app-owned; library stops at framed bytes.", true,
     kReqIds_NtripBoundary, "INtripSink is a documentation stub"},
};

static constexpr size_t kCapabilityCount =
    sizeof(kCapabilities) / sizeof(kCapabilities[0]);

inline const Capability* findCapability(const char* id) {
  if (id == nullptr) return nullptr;
  for (size_t i = 0; i < kCapabilityCount; ++i) {
    const char* a = kCapabilities[i].id;
    const char* b = id;
    while (*a && *b && *a == *b) {
      ++a;
      ++b;
    }
    if (*a == 0 && *b == 0) return &kCapabilities[i];
  }
  return nullptr;
}

// Convenience booleans (mirror capability implemented flags for #if-style checks)
static constexpr bool kCapCrc24q = true;
static constexpr bool kCapCrcTable = true;
static constexpr bool kCapFrameAssembler = true;
static constexpr bool kCapBitWrite = true;
static constexpr bool kCapBitRead = true;
static constexpr bool kCapHub = true;
static constexpr bool kCapPolicy = true;
static constexpr bool kCapRegistry = true;
static constexpr bool kCapStats = true;
static constexpr bool kCapCodec1005 = true;
static constexpr bool kCapCodec1006 = true;
static constexpr bool kCapCodec1033 = true;
static constexpr bool kCapCodecMsm = true;
static constexpr bool kCapSanitize = true;
static constexpr bool kCapGoldens = true;
static constexpr bool kCapSelfTest = true;

}  // namespace tinyrtcm3
