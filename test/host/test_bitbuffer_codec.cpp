#include <cstdio>
#include <cstdint>
#include <cstring>
#include "TinyRtcmBitBuffer.h"
#include "TinyRtcmCodec.h"
#include "TinyRtcmCrc24q.h"

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

static bool put38(BitBuffer& bb, int64_t v) {
  const uint64_t u = static_cast<uint64_t>(v) & ((1ULL << 38) - 1ULL);
  return bb.putBits(static_cast<uint32_t>(u >> 32), 6) == Status::Ok &&
         bb.putBits(static_cast<uint32_t>(u), 32) == Status::Ok;
}

static bool build1005(uint8_t* frame, size_t cap, size_t* n, uint16_t station, int64_t x,
                      int64_t y, int64_t z) {
  uint8_t payload[19] = {};
  BitBuffer bb(payload, sizeof(payload));
  bb.resetWrite();
  if (bb.putBits(1005, 12) != Status::Ok) return false;
  if (bb.putBits(station, 12) != Status::Ok) return false;
  if (bb.putBits(0, 6) != Status::Ok) return false;
  if (bb.putBits(1, 1) != Status::Ok) return false;  // GPS
  if (bb.putBits(0, 1) != Status::Ok) return false;
  if (bb.putBits(0, 1) != Status::Ok) return false;
  if (bb.putBits(0, 1) != Status::Ok) return false;
  if (!put38(bb, x)) return false;
  if (bb.putBits(0, 1) != Status::Ok) return false;  // osc
  if (bb.putBits(0, 1) != Status::Ok) return false;  // res
  if (!put38(bb, y)) return false;
  if (bb.putBits(0, 2) != Status::Ok) return false;  // QCI
  if (!put38(bb, z)) return false;
  if (bb.bitLength() != 152) return false;
  return finalizeFrame(payload, sizeof(payload), frame, cap, n) == Status::Ok;
}

static bool readAll(const char* path, uint8_t* buf, size_t cap, size_t* n) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  const size_t got = std::fread(buf, 1, cap, f);
  std::fclose(f);
  *n = got;
  return got > 0;
}

static const char* findGolden(const char* name, uint8_t* buf, size_t cap, size_t* n) {
  static const char* kRoots[] = {
      "test/golden/synthetic/",
      "../test/golden/synthetic/",
      "../../test/golden/synthetic/",
      "golden/synthetic/",
  };
  char path[256];
  for (const char* root : kRoots) {
    std::snprintf(path, sizeof(path), "%s%s", root, name);
    if (readAll(path, buf, cap, n)) return name;
  }
  return nullptr;
}

int main() {
  // --- putBits ↔ getBits round-trip, including byte-boundary crossing ---
  {
    uint8_t buf[16] = {};
    BitBuffer bb(buf, sizeof(buf));
    bb.resetWrite();
    expect(bb.putBits(0xD3u, 8) == Status::Ok, "put 8");
    expect(bb.putBits(1005u, 12) == Status::Ok, "put 12");
    expect(bb.putBits(0x1FFFFFFu, 25) == Status::Ok, "put 25");
    expect(bb.putBits(0xFFFFFFFFu, 32) == Status::Ok, "put 32");
    bb.resetRead();
    uint32_t v = 0;
    expect(bb.getBits(8, &v) == Status::Ok && v == 0xD3u, "get 8");
    expect(bb.getBits(12, &v) == Status::Ok && v == 1005u, "get 12");
    expect(bb.getBits(25, &v) == Status::Ok && v == 0x1FFFFFFu, "get 25");
    expect(bb.getBits(32, &v) == Status::Ok && v == 0xFFFFFFFFu, "get 32");
  }

  {
    uint32_t v = 0;
    BitBuffer bad(nullptr, 4);
    expect(bad.getBits(8, &v) == Status::InvalidArg, "getBits null data");
    uint8_t buf[2] = {};
    BitBuffer bb(buf, sizeof(buf));
    expect(bb.getBits(0, &v) == Status::InvalidArg, "getBits nbits 0");
    expect(bb.getBits(33, &v) == Status::InvalidArg, "getBits nbits 33");
    expect(bb.getBits(8, nullptr) == Status::InvalidArg, "getBits null out");
    expect(bb.getBits(16, &v) == Status::Ok, "getBits 16 of 16");
    expect(bb.getBits(1, &v) == Status::Overflow, "getBits overflow");
  }

  // --- decode1005 round-trip including negative ECEF ---
  {
    uint8_t frame[32] = {};
    size_t n = 0;
    expect(build1005(frame, sizeof(frame), &n, 42, kPublishArpEcef01mmX, -1, 123456789),
           "build 1005");
    expect(frameCrcOk(frame, n), "constructed 1005 CRC");
    expect(messageType(frame, n) == 1005, "constructed type 1005");
    Msg1005 m;
    expect(decode1005(frame, n, &m) == Status::Ok, "decode constructed 1005");
    expect(m.stationId == 42, "station 42");
    expect(m.ecefX01mm == kPublishArpEcef01mmX, "X dummy");
    expect(m.ecefY01mm == -1, "Y -1 sign-extend");
    expect(m.ecefZ01mm == 123456789, "Z");
  }

  {
    uint8_t frame[32] = {};
    size_t n = 0;
    expect(build1005(frame, sizeof(frame), &n, kPublishStationId, kPublishArpEcef01mmX,
                     kPublishArpEcef01mmY, kPublishArpEcef01mmZ),
           "build dummy ARP");
    uint8_t bad[32];
    std::memcpy(bad, frame, n);
    bad[n - 1] ^= 0x01;
    Msg1005 m;
    expect(decode1005(bad, n, &m) == Status::BadCrc, "decode BadCrc");
    expect(decode1005(nullptr, n, &m) == Status::InvalidArg, "decode null frame");
    expect(decode1005(frame, n, nullptr) == Status::InvalidArg, "decode null out");
    expect(decode1005(frame, 5, &m) == Status::InvalidArg, "decode short");
  }

  // --- synthetic goldens ---
  {
    uint8_t buf[64];
    size_t n = 0;
    expect(findGolden("empty.bin", buf, sizeof(buf), &n) != nullptr, "load empty.bin");
    expect(n == 6 && frameCrcOk(buf, n), "empty.bin CRC");

    expect(findGolden("1005_dummy_arp.bin", buf, sizeof(buf), &n) != nullptr,
           "load 1005_dummy_arp.bin");
    expect(n == 25 && frameCrcOk(buf, n), "1005 golden CRC");
    expect(messageType(buf, n) == 1005, "1005 golden type");
    Msg1005 m;
    expect(decode1005(buf, n, &m) == Status::Ok, "decode 1005 golden");
    expect(m.stationId == kPublishStationId, "golden station 0");
    expect(m.ecefX01mm == kPublishArpEcef01mmX, "golden X");
    expect(m.ecefY01mm == kPublishArpEcef01mmY, "golden Y");
    expect(m.ecefZ01mm == kPublishArpEcef01mmZ, "golden Z");
  }

  return fails ? 1 : 0;
}
