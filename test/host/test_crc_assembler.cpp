#include <cstdio>
#include <cstdint>
#include <cstring>
#include "TinyRtcmCrc24q.h"
#include "TinyRtcmFrameAssembler.h"

using namespace tinyrtcm3;

static int fails = 0;

static void expect(bool ok, const char* msg) {
  if (!ok) {
    std::printf("FAIL: %s\n", msg);
    ++fails;
  } else {
    std::printf("ok: %s\n", msg);
  }
}

int main() {
  // --- create + verify empty payload frame ---
  uint8_t empty[6] = {};
  size_t n = 0;
  expect(finalizeFrame(nullptr, 0, empty, sizeof(empty), &n) == Status::Ok && n == 6,
         "finalizeFrame empty");
  expect(empty[0] == 0xD3 && empty[1] == 0x00 && empty[2] == 0x00, "empty header");
  expect(frameCrcOk(empty, 6), "empty frame CRC ok");

  // Known CRC for D3 00 00 (bit algorithm, init 0, poly 0x1864CFB)
  const uint32_t emptyCrc = loadCrc24q(empty + 3);
  const uint32_t recomputed = crc24q(empty, 3);
  expect(emptyCrc == recomputed, "store/load CRC match compute");

  // --- corrupt CRC fails ---
  uint8_t bad[6];
  memcpy(bad, empty, 6);
  bad[5] ^= 0x01;
  expect(!frameCrcOk(bad, 6), "corrupt CRC rejected");

  // --- finalize non-empty payload + appendCrc path ---
  const uint8_t payload[] = {0x40, 0x00, 0x00};  // type bits look like 100x-ish stub
  uint8_t frame[32];
  expect(finalizeFrame(payload, sizeof(payload), frame, sizeof(frame), &n) == Status::Ok,
         "finalizeFrame payload");
  expect(n == sizeof(payload) + 6, "frame length");
  expect(frameCrcOk(frame, n), "payload frame CRC ok");

  // Rebuild via appendCrc24q only
  uint8_t body[32];
  body[0] = 0xD3;
  body[1] = 0x00;
  body[2] = 0x03;
  memcpy(body + 3, payload, 3);
  size_t n2 = 0;
  expect(appendCrc24q(body, 6, sizeof(body), &n2) == Status::Ok && n2 == 9,
         "appendCrc24q");
  expect(frameCrcOk(body, n2), "appendCrc path CRC ok");
  expect(n2 == n && memcmp(body, frame, n) == 0, "append matches finalize");

  // --- assembler accepts good, rejects bad ---
  FrameAssembler asmblr;
  uint8_t out[32];
  size_t outLen = 0;
  FrameView view;
  Status st = Status::NeedMore;
  for (size_t i = 0; i < n; ++i) {
    st = asmblr.feed(frame[i], out, sizeof(out), &outLen, &view);
  }
  expect(st == Status::Ok && outLen == n, "assembler accepts good CRC");

  asmblr.reset();
  st = Status::NeedMore;
  for (size_t i = 0; i < 6; ++i) {
    st = asmblr.feed(bad[i], out, sizeof(out), &outLen, &view);
  }
  expect(st == Status::BadCrc, "assembler BadCrc on corrupt");

  // Length mismatch rejected by appendCrc
  uint8_t mismatch[16] = {0xD3, 0x00, 0x01, 0x00};
  expect(appendCrc24q(mismatch, 4, sizeof(mismatch), &n2) == Status::InvalidArg,
         "appendCrc length mismatch");

  return fails ? 1 : 0;
}
