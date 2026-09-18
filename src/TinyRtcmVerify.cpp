#include "TinyRtcmVerify.h"
#include "TinyRtcmRequirements.h"
#include "TinyRtcmCrc24q.h"
#include "TinyRtcmFrameAssembler.h"
#include "TinyRtcmBitBuffer.h"
#include "TinyRtcmCodec.h"
#include "TinyRtcmSanitize.h"

namespace tinyrtcm3 {
namespace {

void emit(VerifyLogFn log, void* user, const char* line) {
  if (log) log(line, user);
}
void pass(VerifyReport* r, VerifyLogFn log, void* user, const char* msg) {
  ++r->passed;
  emit(log, user, msg);
}
void fail(VerifyReport* r, VerifyLogFn log, void* user, const char* msg) {
  ++r->failed;
  emit(log, user, msg);
}
void skip(VerifyReport* r, VerifyLogFn log, void* user, const char* msg) {
  ++r->skipped;
  emit(log, user, msg);
}

}  // namespace

int runSelfTests(VerifyReport* report, VerifyLogFn log, void* user) {
  VerifyReport local;
  VerifyReport* r = report ? report : &local;
  r->passed = r->failed = r->skipped = 0;
  emit(log, user, "TinyRTCM3 self-test begin");

  // CAP/REQ catalog sanity: each capability lists at least one known REQ
  {
    bool ok = kCapabilityCount > 0 && kRequirementCount > 0;
    for (size_t i = 0; ok && i < kCapabilityCount; ++i) {
      const Capability& c = kCapabilities[i];
      if (c.reqIds == nullptr || c.reqIds[0] == nullptr) ok = false;
      else if (findRequirement(c.reqIds[0]) == nullptr) ok = false;
    }
    const Capability* crc = findCapability("CAP-CRC-24Q");
    const Requirement* rcrc = findRequirement("REQ-CRC-01");
    const Capability* bitr = findCapability("CAP-BIT-READ");
    const Requirement* rbit = findRequirement("REQ-BIT-02");
    const Requirement* r1005d = findRequirement("REQ-COD-1005-D");
    ok = ok && crc && crc->implemented && rcrc && rcrc->met;
    ok = ok && bitr && bitr->implemented && rbit && rbit->met;
    ok = ok && r1005d && r1005d->met;
    if (ok) pass(r, log, user, "ok CAP/REQ catalog linkage");
    else fail(r, log, user, "FAIL CAP/REQ catalog linkage");
  }

  // CAP-CRC-24Q + CAP-FRAME-ASM
  {
    uint8_t frame[16] = {};
    size_t n = 0;
    if (finalizeFrame(nullptr, 0, frame, sizeof(frame), &n) == Status::Ok && n == 6 &&
        frameCrcOk(frame, 6)) {
      pass(r, log, user, "ok CAP-CRC-24Q finalize empty");
    } else {
      fail(r, log, user, "FAIL CAP-CRC-24Q finalize empty");
    }

    uint8_t bad[6];
    for (size_t i = 0; i < 6; ++i) bad[i] = frame[i];
    bad[5] ^= 0x01;
    if (!frameCrcOk(bad, 6)) pass(r, log, user, "ok CAP-CRC-24Q reject corrupt");
    else fail(r, log, user, "FAIL CAP-CRC-24Q reject corrupt");

    FrameAssembler asmblr;
    uint8_t out[32];
    size_t outLen = 0;
    FrameView view;
    Status st = Status::NeedMore;
    for (size_t i = 0; i < 6; ++i) st = asmblr.feed(frame[i], out, sizeof(out), &outLen, &view);
    if (st == Status::Ok) pass(r, log, user, "ok CAP-FRAME-ASM good frame");
    else fail(r, log, user, "FAIL CAP-FRAME-ASM good frame");

    asmblr.reset();
    st = Status::NeedMore;
    for (size_t i = 0; i < 6; ++i) st = asmblr.feed(bad[i], out, sizeof(out), &outLen, &view);
    if (st == Status::BadCrc) pass(r, log, user, "ok CAP-FRAME-ASM BadCrc");
    else fail(r, log, user, "FAIL CAP-FRAME-ASM BadCrc");
  }

  // CAP-BIT-READ: putBits ↔ getBits round-trip (REQ-BIT-02)
  {
    uint8_t buf[8] = {};
    BitBuffer bb(buf, sizeof(buf));
    bb.resetWrite();
    uint32_t got = 0xFFFFFFFFu;
    const bool ok = bb.putBits(0xABCu, 12) == Status::Ok &&
                    bb.putBits(0x5Au, 8) == Status::Ok &&
                    bb.putBits(1u, 1) == Status::Ok &&
                    (bb.resetRead(), true) && bb.getBits(12, &got) == Status::Ok &&
                    got == 0xABCu && bb.getBits(8, &got) == Status::Ok && got == 0x5Au &&
                    bb.getBits(1, &got) == Status::Ok && got == 1u;
    if (ok) pass(r, log, user, "ok CAP-BIT-READ put/get round-trip");
    else fail(r, log, user, "FAIL CAP-BIT-READ put/get round-trip");

    uint32_t unused = 0;
    const bool args = bb.getBits(0, &unused) == Status::InvalidArg &&
                      bb.getBits(33, &unused) == Status::InvalidArg &&
                      bb.getBits(8, nullptr) == Status::InvalidArg;
    BitBuffer empty(nullptr, 0);
    const bool ovf = empty.getBits(1, &unused) == Status::InvalidArg;
    uint8_t tiny[1] = {0xFF};
    BitBuffer shortBb(tiny, 1);
    shortBb.resetRead();
    const bool past = shortBb.getBits(8, &unused) == Status::Ok &&
                      shortBb.getBits(1, &unused) == Status::Overflow;
    if (args && ovf && past) pass(r, log, user, "ok CAP-BIT-READ InvalidArg/Overflow");
    else fail(r, log, user, "FAIL CAP-BIT-READ InvalidArg/Overflow");
  }

  // REQ-COD-1005-D: construct dummy 1005 via putBits and decode
  {
    uint8_t payload[19] = {};
    BitBuffer bb(payload, sizeof(payload));
    bb.resetWrite();
    auto put38 = [&bb](int64_t v) -> bool {
      const uint64_t u = static_cast<uint64_t>(v) & ((1ULL << 38) - 1ULL);
      return bb.putBits(static_cast<uint32_t>(u >> 32), 6) == Status::Ok &&
             bb.putBits(static_cast<uint32_t>(u), 32) == Status::Ok;
    };
    bool pok = bb.putBits(1005, 12) == Status::Ok &&
               bb.putBits(kPublishStationId, 12) == Status::Ok &&
               bb.putBits(0, 6) == Status::Ok && bb.putBits(1, 1) == Status::Ok &&
               bb.putBits(0, 1) == Status::Ok && bb.putBits(0, 1) == Status::Ok &&
               bb.putBits(0, 1) == Status::Ok && put38(kPublishArpEcef01mmX) &&
               bb.putBits(0, 1) == Status::Ok && bb.putBits(0, 1) == Status::Ok &&
               put38(kPublishArpEcef01mmY) && bb.putBits(0, 2) == Status::Ok &&
               put38(kPublishArpEcef01mmZ) && bb.bitLength() == 152;
    uint8_t frame[32] = {};
    size_t n = 0;
    Msg1005 m;
    pok = pok && finalizeFrame(payload, sizeof(payload), frame, sizeof(frame), &n) == Status::Ok &&
          decode1005(frame, n, &m) == Status::Ok && m.stationId == kPublishStationId &&
          m.ecefX01mm == kPublishArpEcef01mmX && m.ecefY01mm == kPublishArpEcef01mmY &&
          m.ecefZ01mm == kPublishArpEcef01mmZ;
    if (pok) pass(r, log, user, "ok REQ-COD-1005-D decode dummy ARP");
    else fail(r, log, user, "FAIL REQ-COD-1005-D decode dummy ARP");
  }

  // Remaining codec + sanitize stubs expected Unsupported (REQ-VER-02)
  {
    Msg1005 m5;
    Msg1006 m6;
    Msg1033 m3;
    MsmHeaderCnrSummary msm;
    uint8_t buf[64];
    size_t n = 0;
    const bool stubs =
        encode1005(m5, buf, sizeof(buf), &n) == Status::Unsupported &&
        rewrite1005ToPublishIdentity(buf, 0, buf, sizeof(buf), &n) == Status::Unsupported &&
        decode1006(buf, 0, &m6) == Status::Unsupported &&
        encode1006(m6, buf, sizeof(buf), &n) == Status::Unsupported &&
        decode1033(buf, 0, &m3) == Status::Unsupported &&
        encode1033(m3, buf, sizeof(buf), &n) == Status::Unsupported &&
        summarizeMsmCnr(buf, 0, &msm) == Status::Unsupported &&
        sanitizeRewriteLocationFrame(buf, 0, sizeof(buf), &n) == Status::Unsupported;
    if (stubs) skip(r, log, user, "skip remaining CAP-CODEC-* / sanitize rewrite (Unsupported)");
    else fail(r, log, user, "FAIL expected Unsupported stubs");
  }

  emit(log, user, "TinyRTCM3 self-test end");
  return r->failed;
}

}  // namespace tinyrtcm3
