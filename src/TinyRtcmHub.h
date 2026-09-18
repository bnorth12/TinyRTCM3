#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"
#include "TinyRtcmFrameAssembler.h"

namespace tinyrtcm3 {

// Passthrough hub: assemble inbound bytes, optional filter callback, emit frames.
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

  Status feed(uint8_t b) {
    FrameView view;
    size_t n = 0;
    const Status st = asm_.feed(b, frame_, sizeof(frame_), &n, &view);
    if (st != Status::Ok) return st;
    if (filter_ && !filter_(view.messageType, frame_, n, filterCtx_)) {
      return Status::Ok;  // dropped by policy
    }
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
};

}  // namespace tinyrtcm3
