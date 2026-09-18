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
  // Empty payload frame: 0xD3 0x00 0x00 + CRC
  uint8_t empty[6] = {0xD3, 0x00, 0x00, 0, 0, 0};
  const uint32_t c = crc24q(empty, 3);
  empty[3] = static_cast<uint8_t>((c >> 16) & 0xFF);
  empty[4] = static_cast<uint8_t>((c >> 8) & 0xFF);
  empty[5] = static_cast<uint8_t>(c & 0xFF);
  expect(frameCrcOk(empty, 6), "empty frame CRC");

  FrameAssembler asmblr;
  uint8_t out[32];
  size_t n = 0;
  FrameView view;
  Status st = Status::NeedMore;
  for (size_t i = 0; i < 6; ++i) {
    st = asmblr.feed(empty[i], out, sizeof(out), &n, &view);
  }
  expect(st == Status::Ok && n == 6, "assembler reassembles empty frame");

  return fails ? 1 : 0;
}
