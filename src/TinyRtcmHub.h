#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"
#include "TinyRtcmFrameAssembler.h"
#include "TinyRtcmStats.h"

namespace tinyrtcm3 {

// CAP-HUB — assemble -> optional filter -> emit (+ optional StreamStats)
class Hub {
 public:
  using Filter = bool (*)(uint16_t messageType, const uint8_t* frame, size_t len, void* ctx);
  using Emit = void (*)(const uint8_t* frame, size_t len, void* ctx);

  void setFilter(Filter f, void* ctx) {
    filter_ = f;
    filterCtx_ = ctx;
  }
  void setEmit(Emit e, void* ctx) {
    emit_ = e;
    emitCtx_ = ctx;
  }
  // Optional; does not affect assembler state when reset via statsReset.
  void setStats(StreamStats* s) { stats_ = s; }

  Status feed(uint8_t b) {
    statsOnByte(stats_);
    FrameView view;
    size_t n = 0;
    const Status st = asm_.feed(b, frame_, sizeof(frame_), &n, &view);
    if (st == Status::NeedMore) return st;
    if (st != Status::Ok) {
      statsOnStatus(stats_, st, false);
      return st;
    }
    if (filter_ && !filter_(view.messageType, frame_, n, filterCtx_)) {
      statsOnStatus(stats_, Status::Ok, true);
      return Status::Ok;
    }
    statsOnStatus(stats_, Status::Ok, false);
    if (emit_) emit_(frame_, n, emitCtx_);
    return Status::Ok;
  }

 private:
  FrameAssembler asm_;
  uint8_t frame_[1100];
  Filter filter_ = nullptr;
  void* filterCtx_ = nullptr;
  Emit emit_ = nullptr;
  void* emitCtx_ = nullptr;
  StreamStats* stats_ = nullptr;
};

}  // namespace tinyrtcm3
