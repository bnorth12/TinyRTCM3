#include "TinyRtcmVerify.h"
#include "TinyRtcmRequirements.h"
#include "TinyRtcmCrc24q.h"
#include "TinyRtcmFrameAssembler.h"
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
    ok = ok && crc && crc->implemented && rcrc && rcrc->met;
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

  // Codec + sanitize stubs expected Unsupported (REQ-VER-02)
  {
    Msg1005 m5;
    Msg1006 m6;
    Msg1033 m3;
    MsmHeaderCnrSummary msm;
    uint8_t buf[64];
    size_t n = 0;
    const bool stubs =
        decode1005(buf, 0, &m5) == Status::Unsupported &&
        encode1005(m5, buf, sizeof(buf), &n) == Status::Unsupported &&
        rewrite1005ToPublishIdentity(buf, 0, buf, sizeof(buf), &n) == Status::Unsupported &&
        decode1006(buf, 0, &m6) == Status::Unsupported &&
        encode1006(m6, buf, sizeof(buf), &n) == Status::Unsupported &&
        decode1033(buf, 0, &m3) == Status::Unsupported &&
        encode1033(m3, buf, sizeof(buf), &n) == Status::Unsupported &&
        summarizeMsmCnr(buf, 0, &msm) == Status::Unsupported &&
        sanitizeRewriteLocationFrame(buf, 0, sizeof(buf), &n) == Status::Unsupported;
    if (stubs) skip(r, log, user, "skip CAP-CODEC-* / sanitize rewrite (Unsupported stubs)");
    else fail(r, log, user, "FAIL expected Unsupported stubs");
  }

  emit(log, user, "TinyRTCM3 self-test end");
  return r->failed;
}

}  // namespace tinyrtcm3
