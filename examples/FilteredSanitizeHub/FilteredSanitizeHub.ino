#include <Arduino.h>
#include <TinyRTCM3.h>

using namespace tinyrtcm3;

// Solid-library example: Policy ISO-only OR KeepStationAndMsm + sanitize-before-emit.
// No Registry wiring (deferred). Demonstrates privacy path for public sinks.

static Hub hub;
static StreamStats stats;
static SanitizeEmitCtx sanitizeCtx;

static void onPublicEmit(const uint8_t* frame, size_t len, void*) {
  // After sanitize: safe for Serial1 / NTRIP / log
  Serial1.write(frame, len);
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
  while (Serial.available()) {
    const int n = Serial.available();
    uint8_t buf[64];
    const int got = Serial.readBytes(reinterpret_cast<char*>(buf), n > 64 ? 64 : n);
    if (got > 0) hub.feed(buf, static_cast<size_t>(got), nullptr);
  }
}