#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"
#include "TinyRtcmCodec.h"
#include "TinyRtcmCrc24q.h"

namespace tinyrtcm3 {

// CAP-SANITIZE — classify/drop/rewrite location messages for public export.
// REQ-SAN-01/02/03 (sanitize-before-emit helper)

enum class SanitizeAction : uint8_t { Pass = 0, Drop, Rewrite };

inline SanitizeAction sanitizeClassify(uint16_t messageType) {
  if (messageType == 1005 || messageType == 1006 || messageType == 1033)
    return SanitizeAction::Rewrite;
  return SanitizeAction::Pass;
}

Status sanitizeRewriteLocationFrame(uint8_t* frame, size_t len, size_t cap, size_t* outLen);

// Hub Emit-compatible adapter: rewrite 1005/1006/1033 then forward (REQ-SAN-03).
using FrameEmitFn = void (*)(const uint8_t* frame, size_t len, void* ctx);

struct SanitizeEmitCtx {
  FrameEmitFn next = nullptr;
  void* nextCtx = nullptr;
  uint8_t scratch[1100] = {};
};

inline void sanitizeBeforeEmit(const uint8_t* frame, size_t len, void* ctx) {
  if (ctx == nullptr || frame == nullptr || len == 0) return;
  auto* c = static_cast<SanitizeEmitCtx*>(ctx);
  if (c->next == nullptr) return;
  if (len < kRtcmMinFrameLen || len > sizeof(c->scratch)) {
    c->next(frame, len, c->nextCtx);
    return;
  }
  const uint16_t t = messageType(frame, len);
  if (sanitizeClassify(t) != SanitizeAction::Rewrite) {
    c->next(frame, len, c->nextCtx);
    return;
  }
  for (size_t i = 0; i < len; ++i) c->scratch[i] = frame[i];
  size_t outLen = 0;
  if (sanitizeRewriteLocationFrame(c->scratch, len, sizeof(c->scratch), &outLen) != Status::Ok) {
    return;  // drop on sanitize failure (do not leak)
  }
  c->next(c->scratch, outLen, c->nextCtx);
}

}  // namespace tinyrtcm3