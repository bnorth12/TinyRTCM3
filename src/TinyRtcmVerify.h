#pragma once
#include <stddef.h>
#include <stdint.h>
#include "TinyRtcmRequirements.h"

namespace tinyrtcm3 {

// Optional line logger (Arduino Serial wrapper, host printf, etc.).
using VerifyLogFn = void (*)(const char* line, void* user);

struct VerifyReport {
  int passed = 0;
  int failed = 0;
  int skipped = 0;  // known-unimplemented Codec stubs exercised as Unsupported
};

// Development self-test for implemented capabilities (CRC/assembler/getBits/
// decode1005) and remaining Codec Unsupported skips. Returns failed count
// (0 = success). No heap allocation. Safe for host CI or Arduino SelfTest.
int runSelfTests(VerifyReport* report = nullptr, VerifyLogFn log = nullptr,
                 void* user = nullptr);

}  // namespace tinyrtcm3
