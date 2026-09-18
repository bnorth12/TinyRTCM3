#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"
#include "TinyRtcmFrameAssembler.h"

namespace tinyrtcm3 {

// =============================================================================
// CAP-HUB — assemble → optional filter → emit
// Requirements: REQ-HUB-01, REQ-HUB-02
// Intent: the application-facing RTCM pipe between a byte source (UART) and a
// byte sink (NTRIP sink stub, radio, SD log). Filtering is policy (see
// TinyRtcmPolicy.h); this class does not interpret message bodies.
// =============================================================================
class Hub {
 public:
  // Return true to keep/emit the frame; false to drop (still Status::Ok — REQ-HUB-02).
  using Filter = bool (*)(uint16_t messageType, const uint8_t* frame, size_t len, void* ctx);
  // Called with a complete CRC-valid frame (preamble..CRC).
  using Emit = void (*)(const uint8_t* frame, size_t len, void* ctx);

  void setFilter(Filter f, void* ctx) {
    filter_ = f;
    filterCtx_ = ctx;
  }
  void setEmit(Emit e, void* ctx) {
    emit_ = e;
    emitCtx_ = ctx;
  }

  // Feed one inbound byte. Returns NeedMore until a frame completes, Ok after
  // emit/drop, or BadCrc/Overflow from the assembler.
  Status feed(uint8_t b) {
    FrameView view;
    size_t n = 0;
    const Status st = asm_.feed(b, frame_, sizeof(frame_), &n, &view);
    if (st != Status::Ok) return st;
    if (filter_ && !filter_(view.messageType, frame_, n, filterCtx_)) {
      return Status::Ok;  // dropped by policy (not an error)
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
