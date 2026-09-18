#include "TinyRtcmVerify.h"
#include "TinyRtcmRequirements.h"
#include "TinyRtcmCrc24q.h"
#include "TinyRtcmFrameAssembler.h"
#include "TinyRtcmBitBuffer.h"
#include "TinyRtcmCodec.h"
#include "TinyRtcmSanitize.h"
#include "TinyRtcmPolicy.h"
#include "TinyRtcmRegistry.h"
#include "TinyRtcmStats.h"

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

  // v0.2: encode1005 + rewrite + sanitize 1005
  {
    Msg1005 m;
    m.stationId = 42;
    m.ecefX01mm = kPublishArpEcef01mmX;
    m.ecefY01mm = -1;
    m.ecefZ01mm = kPublishArpEcef01mmZ;
    uint8_t frame[64];
    size_t n = 0;
    Msg1005 back;
    bool ok = encode1005(m, frame, sizeof(frame), &n) == Status::Ok && n >= 6 &&
              decode1005(frame, n, &back) == Status::Ok && back.stationId == 42 &&
              back.ecefX01mm == m.ecefX01mm && back.ecefY01mm == -1 &&
              back.ecefZ01mm == m.ecefZ01mm;
    if (ok) pass(r, log, user, "ok REQ-COD-1005-E encode/decode round-trip");
    else fail(r, log, user, "FAIL REQ-COD-1005-E encode/decode round-trip");

    Msg1005 dirty = m;
    dirty.stationId = 99;
    dirty.ecefX01mm = 123456789LL;
    ok = encode1005(dirty, frame, sizeof(frame), &n) == Status::Ok;
    uint8_t out[64];
    size_t on = 0;
    Msg1005 pub;
    ok = ok && rewrite1005ToPublishIdentity(frame, n, out, sizeof(out), &on) == Status::Ok &&
         decode1005(out, on, &pub) == Status::Ok && pub.stationId == kPublishStationId &&
         pub.ecefX01mm == kPublishArpEcef01mmX && pub.ecefY01mm == kPublishArpEcef01mmY &&
         pub.ecefZ01mm == kPublishArpEcef01mmZ;
    if (ok) pass(r, log, user, "ok REQ-COD-1005-R rewrite to publish identity");
    else fail(r, log, user, "FAIL REQ-COD-1005-R rewrite to publish identity");

    size_t sn = 0;
    ok = sanitizeRewriteLocationFrame(frame, n, sizeof(frame), &sn) == Status::Ok &&
         decode1005(frame, sn, &pub) == Status::Ok && pub.stationId == kPublishStationId;
    if (ok) pass(r, log, user, "ok REQ-SAN-02 sanitize rewrite 1005");
    else fail(r, log, user, "FAIL REQ-SAN-02 sanitize rewrite 1005");
  }

  // Policy + registry + stats smoke
  {
    const bool pol = policyIsStationIdentityType(1005) && policyIsMsmType(1077) &&
                     policyFilterKeepStationAndMsm(1005, nullptr, 0, nullptr) &&
                     !policyFilterDropMsm(1077, nullptr, 0, nullptr);
    if (pol) pass(r, log, user, "ok CAP-POLICY stock predicates");
    else fail(r, log, user, "FAIL CAP-POLICY stock predicates");

    MessageRegistry reg;
    const RegistryEntry table[] = {{1005, nullptr}};
    reg.setTable(table, 1);
    const bool regOk = reg.dispatch(9999, nullptr, 0, nullptr) == Status::Unsupported;
    if (regOk) pass(r, log, user, "ok CAP-REGISTRY unknown -> Unsupported");
    else fail(r, log, user, "FAIL CAP-REGISTRY unknown");

    StreamStats s;
    statsOnByte(&s);
    statsOnByte(&s);
    statsOnStatus(&s, Status::Ok, false);
    statsOnStatus(&s, Status::Ok, true);
    statsOnStatus(&s, Status::BadCrc, false);
    const bool stOk = s.bytesIn == 2 && s.framesOk == 1 && s.framesDroppedByFilter == 1 &&
                      s.framesBadCrc == 1;
    statsReset(&s);
    const bool resetOk = s.bytesIn == 0 && s.framesOk == 0;
    if (stOk && resetOk) pass(r, log, user, "ok CAP-STATS counters + reset");
    else fail(r, log, user, "FAIL CAP-STATS");
  }

  // v0.3: 1006 / 1033 encode-decode + MSM still Unsupported
  {
    Msg1006 m6;
    m6.stationId = 7;
    m6.ecefX01mm = kPublishArpEcef01mmX;
    m6.ecefY01mm = -2;
    m6.ecefZ01mm = kPublishArpEcef01mmZ;
    m6.antennaHeight01mm = 15000;
    uint8_t frame[64];
    size_t n = 0;
    Msg1006 back;
    bool ok = encode1006(m6, frame, sizeof(frame), &n) == Status::Ok && n == 27 &&
              decode1006(frame, n, &back) == Status::Ok && back.stationId == 7 &&
              back.ecefX01mm == m6.ecefX01mm && back.ecefY01mm == -2 &&
              back.ecefZ01mm == m6.ecefZ01mm && back.antennaHeight01mm == 15000;
    if (ok) pass(r, log, user, "ok REQ-COD-1006-E/D encode/decode round-trip");
    else fail(r, log, user, "FAIL REQ-COD-1006-E/D encode/decode round-trip");

    Msg1033 m3;
    m3.stationId = 3;
    m3.antennaDescriptor[0] = 'A';
    m3.antennaDescriptor[1] = 'N';
    m3.antennaDescriptor[2] = 'T';
    m3.receiverDescriptor[0] = 'R';
    m3.receiverDescriptor[1] = 'C';
    m3.receiverDescriptor[2] = 'V';
    Msg1033 b3;
    ok = encode1033(m3, frame, sizeof(frame), &n) == Status::Ok && n >= 6 &&
         decode1033(frame, n, &b3) == Status::Ok && b3.stationId == 3 &&
         b3.antennaDescriptor[0] == 'A' && b3.antennaDescriptor[1] == 'N' &&
         b3.antennaDescriptor[2] == 'T' && b3.receiverDescriptor[0] == 'R' &&
         b3.receiverDescriptor[1] == 'C' && b3.receiverDescriptor[2] == 'V';
    if (ok) pass(r, log, user, "ok REQ-COD-1033-E/D encode/decode round-trip");
    else fail(r, log, user, "FAIL REQ-COD-1033-E/D encode/decode round-trip");

    // Minimal GPS MSM4: 2 sats, 2 sigs, 3 cells, CNRs 40/50/45 -> mean 450
    uint8_t msmPayload[64] = {};
    BitBuffer mb(msmPayload, sizeof(msmPayload));
    mb.resetWrite();
    auto putN = [&mb](uint32_t v, uint8_t n) -> bool { return mb.putBits(v, n) == Status::Ok; };
    bool mok = putN(1074, 12) && putN(0, 12) && putN(0, 30) && putN(0, 19) &&
               putN(0xC0000000u, 32) && putN(0, 32) && putN(0xC0000000u, 32) &&
               putN(0xDu, 4) && putN(0, 18) && putN(0, 18);
    const uint32_t cnrs[3] = {40u, 50u, 45u};
    for (int ci = 0; ci < 3; ++ci) {
      mok = mok && putN(0, 32) && putN(0, 10) && putN(cnrs[ci], 6);
    }
    uint8_t msmFrame[96];
    size_t mn = 0;
    MsmHeaderCnrSummary msm;
    mok = mok && finalizeFrame(msmPayload, mb.byteLength(), msmFrame, sizeof(msmFrame), &mn) ==
                     Status::Ok &&
          summarizeMsmCnr(msmFrame, mn, &msm) == Status::Ok && msm.messageType == 1074 &&
          msm.satCount == 2 && msm.sigCount == 2 && msm.meanCnr01dBHz == 450;
    if (mok) pass(r, log, user, "ok REQ-COD-MSM-S MSM4 mean CNR");
    else fail(r, log, user, "FAIL REQ-COD-MSM-S MSM4 mean CNR");
  }

  emit(log, user, "TinyRTCM3 self-test end");
  return r->failed;
}

}  // namespace tinyrtcm3
