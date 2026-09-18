#include <Arduino.h>
#include <TinyRTCM3.h>

using namespace tinyrtcm3;

static Hub hub;

static void onEmit(const uint8_t* frame, size_t len, void*) {
  // Passthrough to Serial1 (NTRIP client / radio / etc.)
  Serial1.write(frame, len);
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  hub.setEmit(onEmit, nullptr);
  // Optional: drop MSM, keep 1005/1006/1033 only — wire filter later.
}

void loop() {
  while (Serial.available()) {
    hub.feed(static_cast<uint8_t>(Serial.read()));
  }
}
