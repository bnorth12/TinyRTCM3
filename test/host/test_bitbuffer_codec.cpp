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


  // --- v0.3 synthetic 1006 / 1033 ---
  {
    uint8_t buf[128];
    size_t n = 0;
    expect(findGolden("1006_dummy_arp.bin", buf, sizeof(buf), &n) != nullptr,
           "load 1006_dummy_arp.bin");
    expect(n == 27 && frameCrcOk(buf, n), "1006 golden CRC");
    expect(messageType(buf, n) == 1006, "1006 golden type");
    Msg1006 m6;
    expect(decode1006(buf, n, &m6) == Status::Ok, "decode 1006 golden");
    expect(m6.stationId == kPublishStationId, "1006 station 0");
    expect(m6.ecefX01mm == kPublishArpEcef01mmX, "1006 X");
    expect(m6.ecefY01mm == kPublishArpEcef01mmY, "1006 Y");
    expect(m6.ecefZ01mm == kPublishArpEcef01mmZ, "1006 Z");
    expect(m6.antennaHeight01mm == 15000, "1006 height 1.5m");

    Msg1006 enc = m6;
    enc.stationId = 11;
    enc.antennaHeight01mm = 42;
    uint8_t frame[64];
    size_t fn = 0;
    expect(encode1006(enc, frame, sizeof(frame), &fn) == Status::Ok, "encode1006");
    Msg1006 back;
    expect(decode1006(frame, fn, &back) == Status::Ok && back.stationId == 11 &&
               back.antennaHeight01mm == 42,
           "1006 encode/decode");

    expect(findGolden("1033_sanitized.bin", buf, sizeof(buf), &n) != nullptr,
           "load 1033_sanitized.bin");
    expect(frameCrcOk(buf, n), "1033 golden CRC");
    expect(messageType(buf, n) == 1033, "1033 golden type");
    Msg1033 m3;
    expect(decode1033(buf, n, &m3) == Status::Ok, "decode 1033 golden");
    expect(m3.stationId == kPublishStationId, "1033 station 0");
    expect(m3.antennaDescriptor[0] == 'A' && m3.antennaDescriptor[1] == 'N' &&
               m3.antennaDescriptor[2] == 'T',
           "1033 ant ANT");
    expect(m3.receiverDescriptor[0] == 'R' && m3.receiverDescriptor[1] == 'C' &&
               m3.receiverDescriptor[2] == 'V',
           "1033 rx RCV");
  }


  // --- v0.4 MSM summary ---
  {
    uint8_t buf[256];
    size_t n = 0;
    expect(findGolden("msm4_cnr_mean.bin", buf, sizeof(buf), &n) != nullptr,
           "load msm4_cnr_mean.bin");
    expect(frameCrcOk(buf, n), "msm4 synthetic CRC");
    MsmHeaderCnrSummary s;
    expect(summarizeMsmCnr(buf, n, &s) == Status::Ok, "summarize synthetic MSM4");
    expect(s.messageType == 1074, "msm type 1074");
    expect(s.satCount == 2 && s.sigCount == 2, "msm sat/sig counts");
    expect(s.meanCnr01dBHz == 450, "msm mean CNR 450");

    // Field samples (optional paths)
    static const char* kFieldRoots[] = {
        "test/golden/field/",
        "../test/golden/field/",
        "../../test/golden/field/",
        "golden/field/",
    };
    auto loadField = [&](const char* name) -> bool {
      char path[256];
      for (const char* root : kFieldRoots) {
        std::snprintf(path, sizeof(path), "%s%s", root, name);
        if (readAll(path, buf, sizeof(buf), &n)) return true;
      }
      return false;
    };
    if (loadField("msm4_1074_sample.bin")) {
      expect(frameCrcOk(buf, n), "field msm4 CRC");
      expect(summarizeMsmCnr(buf, n, &s) == Status::Ok, "summarize field msm4");
      expect(s.messageType == 1074 && s.satCount > 0, "field msm4 header");
      expect(s.meanCnr01dBHz != 0xFFFF, "field msm4 has CNR");
    } else {
      std::printf("skip: field msm4_1074_sample.bin not present\n");
    }
    if (loadField("msm7_1077_sample.bin")) {
      expect(frameCrcOk(buf, n), "field msm7 CRC");
      expect(summarizeMsmCnr(buf, n, &s) == Status::Ok, "summarize field msm7");
      expect(s.messageType == 1077 && s.satCount > 0, "field msm7 header");
    } else {
      std::printf("skip: field msm7_1077_sample.bin not present\n");
    }
  }

  return fails ? 1 : 0;
}
