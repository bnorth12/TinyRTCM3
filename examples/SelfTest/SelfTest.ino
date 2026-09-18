#include <Arduino.h>
#include <TinyRTCM3.h>

using namespace tinyrtcm3;

static void logLine(const char* line, void*) {
  Serial.println(line);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  VerifyReport report;
  const int failed = runSelfTests(&report, logLine, nullptr);
  Serial.print("passed=");
  Serial.print(report.passed);
  Serial.print(" failed=");
  Serial.print(report.failed);
  Serial.print(" skipped=");
  Serial.println(report.skipped);
  Serial.println(failed == 0 ? "SELFTEST OK" : "SELFTEST FAIL");
}

void loop() {}
