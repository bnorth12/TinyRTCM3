#include <cstdio>
#include "TinyRtcmVerify.h"

using namespace tinyrtcm3;

static void logLine(const char* line, void*) {
  std::puts(line);
}

int main() {
  VerifyReport report;
  const int failed = runSelfTests(&report, logLine, nullptr);
  std::printf("summary passed=%d failed=%d skipped=%d\n", report.passed, report.failed,
              report.skipped);
  return failed == 0 ? 0 : 1;
}
