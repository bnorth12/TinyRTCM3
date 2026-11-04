#include <Arduino.h>
#include <TinyRTCM3.h>

using namespace tinyrtcm3;

// Solid-library example: Policy ISO-only OR KeepStationAndMsm + sanitize-before-emit.
// No Registry wiring (deferred). Demonstrates privacy path for public sinks.

static Hub hub;
static StreamStats stats;
static SanitizeEmitCtx sanitizeCtx;
static uint32_t txDrops = 0;

static void onPublicEmit(const uint8_t* frame, size_t len, void*) {
  const int space = Serial1.availableForWrite();
  if (space < static_cast<int>(len) || Serial1.write(frame, len) != len) {
    ++txDrops;
    return;
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  hub.setStats(&stats);
  // Rover-lite diet; swap to policyFilterIsoOnly for station-identity-only.
  hub.setFilter(policyFilterKeepStationAndMsm, nullptr);
  sanitizeCtx.next = onPublicEmit;
  sanitizeCtx.nextCtx = nullptr;
  hub.setEmit(sanitizeBeforeEmit, &sanitizeCtx);
}

void loop() {
  hub.setNowMs(millis());
  while (Serial.available()) {
    const int n = Serial.available();
    uint8_t buf[64];
    const int got = Serial.readBytes(reinterpret_cast<char*>(buf), n > 64 ? 64 : n);
    if (got > 0) hub.feed(buf, static_cast<size_t>(got), nullptr);
  }
  static uint32_t last = 0;
  if (millis() - last < 1000) return;
  last = millis();
  CoverageFlag flags[kOnocoyRequiredCount];
  coverageCheck(&stats, millis(), kDefaultLateAfterMs, flags, kOnocoyRequiredCount);
  uint8_t missing = 0;
  for (size_t i = 0; i < kOnocoyRequiredCount; ++i) {
    if (flags[i].missing) ++missing;
  }
  Serial.print("crcPassRows=");
  Serial.print(stats.typeRowCount);
  Serial.print(" frames=");
  Serial.print(stats.framesOk);
  Serial.print(" txDrops=");
  Serial.print(txDrops);
  Serial.print(" onocoyMissing=");
  Serial.println(missing);
}