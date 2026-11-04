#include <Arduino.h>
#include <TinyRTCM3.h>

// v0.6: whole-frame queue, per-type stats, Onocoy coverage, RAM decode mask.
// The mask is not saved. 1005/1006/1230 stay preferred under queue pressure.

using namespace tinyrtcm3;

static Hub hub;
static StreamStats stats;
static FrameQueue<8, 1030> queue;
static DecodeView views;
static uint32_t txDrops = 0;
static bool decode1005On = false;

static void onEmit(const uint8_t* frame, size_t len, void*) {
  const uint16_t type = messageType(frame, len);
  queue.push(frame, len, type);
  decodeIfEnabled(&stats, frame, len, type, &views);
}

static void sinkHead() {
  if (queue.count() == 0) return;
  const int space = Serial1.availableForWrite();
  uint8_t buf[1030];
  if (space <= 0) {
    if (queue.pull(buf, sizeof(buf)) > 0) ++txDrops;
    return;
  }
  size_t n = queue.pull(buf, static_cast<size_t>(space));
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
  // Runtime decode only. Not written anywhere. ASCII 't' on Serial toggles 1005.
  decode1005On = true;
  decodeMaskSet(&stats, 1005, true);
}

void loop() {
  hub.setNowMs(millis());
  while (Serial.available()) {
    const int raw = Serial.read();
    if (raw < 0) break;
    if (raw == 't') {
      decode1005On = !decode1005On;
      decodeMaskSet(&stats, 1005, decode1005On);
      continue;
    }
    hub.feed(static_cast<uint8_t>(raw));
  }

  sinkHead();

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 1000) return;
  lastPrint = millis();

  CoverageFlag flags[kOnocoyRequiredCount];
  coverageCheck(&stats, millis(), kDefaultLateAfterMs, flags, kOnocoyRequiredCount);
  Serial.print("queue=");
  Serial.print(queue.count());
  Serial.print(" unhealthy=");
  Serial.print(queue.unhealthy() ? 1 : 0);
  Serial.print(" drops=");
  Serial.print(queue.unhealthyDropFrames());
  Serial.print(" txDrops=");
  Serial.print(txDrops);
  Serial.print(" frames=");
  Serial.print(stats.framesOk);
  Serial.print(" decode1005=");
  Serial.print(decode1005On ? 1 : 0);
  Serial.print(" rows=");
  Serial.println(stats.typeRowCount);
  for (size_t i = 0; i < kOnocoyRequiredCount; ++i) {
    if (!flags[i].missing && !flags[i].late) continue;
    Serial.print(" onocoy ");
    Serial.print(flags[i].messageType);
    Serial.println(flags[i].missing ? " missing" : " late");
  }
  if (views.has1005) {
    Serial.print("arp1005 x=");
    Serial.println(static_cast<long>(views.arp1005.ecefX01mm));
  }
}
