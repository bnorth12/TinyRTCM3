#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "TinyRtcmTypes.h"
#include "TinyRtcmCrc24q.h"

namespace tinyrtcm3 {

// Byte-stream RTCM3 assembler (0xD3 + 10-bit length + payload + CRC24Q).
class FrameAssembler {
 public:
  void reset() {
    len_ = 0;
    expected_ = 0;
    phase_ = 0;
  }

  // Returns Ok when a complete CRC-valid frame is in out[]; NeedMore otherwise.
  Status feed(uint8_t b, uint8_t* out, size_t outCap, size_t* outLen, FrameView* view = nullptr) {
    if (out == nullptr || outLen == nullptr) return Status::InvalidArg;
    if (phase_ == 0) {
      if (b != 0xD3) return Status::NeedMore;
      buf_[0] = b;
      len_ = 1;
      phase_ = 1;
      return Status::NeedMore;
    }
    if (len_ >= sizeof(buf_)) {
      reset();
      return Status::Overflow;
    }
    buf_[len_++] = b;
    if (phase_ == 1) {
      phase_ = 2;
      return Status::NeedMore;
    }
    if (phase_ == 2) {
      const size_t payload =
          (static_cast<size_t>(buf_[1] & 0x03u) << 8) | static_cast<size_t>(buf_[2]);
      expected_ = payload + 6u;
      if (expected_ < 6u || expected_ > sizeof(buf_)) {
        reset();
        return Status::Overflow;
      }
      phase_ = 3;
      return Status::NeedMore;
    }
    if (phase_ == 3 && len_ >= expected_) {
      if (!frameCrcOk(buf_, expected_)) {
        reset();
        return Status::BadCrc;
      }
      if (outCap < expected_) {
        reset();
        return Status::Overflow;
      }
      memcpy(out, buf_, expected_);
      *outLen = expected_;
      if (view) {
        view->data = out;
        view->length = expected_;
        view->messageType = messageType(out, expected_);
      }
      reset();
      return Status::Ok;
    }
    return Status::NeedMore;
  }

 private:
  uint8_t buf_[1100];
  size_t len_ = 0;
  size_t expected_ = 0;
  uint8_t phase_ = 0;
};

}  // namespace tinyrtcm3
