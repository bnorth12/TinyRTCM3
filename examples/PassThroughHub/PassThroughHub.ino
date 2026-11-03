#include <Arduino.h>
#include <TinyRTCM3.h>

using namespace tinyrtcm3;

static Hub hub;
static StreamStats stats;
static FrameQueue<8, 1030> queue;

static void onEmit(const uint8_t* frame, size_t len, void*) {
  queue.push(frame, len, messageType(frame, len));
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  hub.setStats(&stats);
  hub.setEmit(onEmit, nullptr);
}

void loop() {
  hub.setNowMs(millis());
  while (Serial.available()) {
    hub.feed(static_cast<uint8_t>(Serial.read()));
  }
  uint8_t buf[1030];
  const size_t n = queue.pull(buf, sizeof(buf));
  if (n > 0) Serial1.write(buf, n);
}
