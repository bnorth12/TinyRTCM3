#include <Arduino.h>
#include <TinyRTCM3.h>

using namespace tinyrtcm3;

static Hub hub;
static StreamStats stats;
static FrameQueue<8, 1030> queue;
static uint32_t txDrops = 0;

static void onEmit(const uint8_t* frame, size_t len, void*) {
  queue.push(frame, len, messageType(frame, len));
}

static void sinkHead() {
  if (queue.count() == 0) return;
  const int space = Serial1.availableForWrite();
  uint8_t buf[1030];
  if (space <= 0) {
    if (queue.pull(buf, sizeof(buf)) > 0) ++txDrops;
    return;
  }
  const size_t n = queue.pull(buf, static_cast<size_t>(space));
  if (n == 0) {
    if (queue.pull(buf, sizeof(buf)) > 0) ++txDrops;
    return;
  }
  if (Serial1.write(buf, n) != n) ++txDrops;
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
  sinkHead();
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 1000) return;
  lastPrint = millis();
  Serial.print("frames=");
  Serial.print(stats.framesOk);
  Serial.print(" txDrops=");
  Serial.print(txDrops);
  Serial.print(" queueDrops=");
  Serial.println(queue.unhealthyDropFrames());
}
