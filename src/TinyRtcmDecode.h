#pragma once
// RAM-only decode views. The mask lives on StreamStats and defaults to all off.
// Nothing in this header is written to NVS.
#include "TinyRtcmCodec.h"
#include "TinyRtcmStats.h"
#include "TinyRtcmPolicy.h"

namespace tinyrtcm3 {

struct DecodeView {
  bool has1005 = false;
  bool has1006 = false;
  bool has1033 = false;
  bool hasMsm = false;
  bool has1230 = false;
  Msg1005 arp1005{};
  Msg1006 arp1006{};
  Msg1033 desc1033{};
  MsmHeaderView msm{};
  Msg1230 glo{};
};

// No-op when the mask bit is clear. Returns the codec status when the bit is set.
inline Status decodeIfEnabled(const StreamStats* stats, const uint8_t* frame, size_t len,
                              uint16_t messageType, DecodeView* view) {
  if (view == nullptr) return Status::InvalidArg;
  if (!decodeMaskGet(stats, messageType)) return Status::Ok;
  if (messageType == 1005) {
    const Status st = decode1005(frame, len, &view->arp1005);
    if (st == Status::Ok) view->has1005 = true;
    return st;
  }
  if (messageType == 1006) {
    const Status st = decode1006(frame, len, &view->arp1006);
    if (st == Status::Ok) view->has1006 = true;
    return st;
  }
  if (messageType == 1033) {
    const Status st = decode1033(frame, len, &view->desc1033);
    if (st == Status::Ok) view->has1033 = true;
    return st;
  }
  if (messageType == 1230) {
    const Status st = decode1230(frame, len, &view->glo);
    if (st == Status::Ok) view->has1230 = true;
    return st;
  }
  if (policyIsMsmWide(messageType)) {
    const Status st = decodeMsmHeader(frame, len, &view->msm);
    if (st == Status::Ok) view->hasMsm = true;
    return st;
  }
  return Status::Ok;
}

}  // namespace tinyrtcm3
