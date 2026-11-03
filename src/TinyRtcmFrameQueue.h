#pragma once
// CAP-FRAMEQUEUE — fixed ring of whole RTCM3 frames. No heap on the hot path.
// The app owns the queue object. This is not the UART byte ring (that stays in
// the pump). pull() copies one head frame only when it fits the caller budget.
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"
#include "TinyRtcmPolicy.h"

namespace tinyrtcm3 {

enum class DropReason : uint8_t {
  None = 0,
  Oversize = 1,             // longer than MaxFrame, or empty/null
  QueueFull = 2,            // no MSM epoch could be evicted
  MsmEpoch = 3,             // a whole MSM epoch was dropped
  ProtectedKept = 4,        // incoming 1005/1006/1230 dropped; queued copies kept
  ReplacedProtected = 5,    // same-type protected slot overwritten; not unhealthy
};

struct DropCounters {
  uint32_t frames = 0;
  uint32_t bytes = 0;
  uint32_t byOversize = 0;
  uint32_t byQueueFull = 0;
  uint32_t byMsmEpoch = 0;
  uint32_t byProtectedKept = 0;
  uint32_t byReplacedProtected = 0;
};

struct FrameQueueConfig {
  uint8_t windowN = 32;       // push samples in the sliding window (1..32)
  uint8_t dropThreshold = 8;  // unhealthy when window drops reach this
};

// 1005 / 1006 / 1230 are never the first eviction choice.
inline bool frameQueueIsProtected(uint16_t messageType) {
  return messageType == 1005 || messageType == 1006 || messageType == 1230;
}

// DF004 epoch (30 bits) at payload bit 24. Returns 0 when the frame is short.
inline uint32_t frameQueueMsmEpoch30(const uint8_t* frame, size_t len) {
  if (frame == nullptr || len < 10) return 0;
  return (static_cast<uint32_t>(frame[6]) << 22) |
         (static_cast<uint32_t>(frame[7]) << 14) |
         (static_cast<uint32_t>(frame[8]) << 6) |
         (static_cast<uint32_t>(frame[9]) >> 2);
}

// Depth slots, each holding one frame of at most MaxFrame bytes (header+CRC).
// 1030 covers the RTCM max frame (1023 payload + 6).
template <size_t Depth = 12, size_t MaxFrame = 1030>
class FrameQueue {
 public:
  static constexpr size_t kDepth = Depth;
  static constexpr size_t kMaxFrame = MaxFrame;
  static constexpr size_t kWindowCap = 32;
  static_assert(Depth >= 1 && Depth <= 64, "FrameQueue depth");
  static_assert(MaxFrame >= 6 && MaxFrame <= 1030, "FrameQueue frame cap");

  void setConfig(FrameQueueConfig c) {
    if (c.windowN == 0 || c.windowN > kWindowCap) c.windowN = static_cast<uint8_t>(kWindowCap);
    if (c.dropThreshold == 0) c.dropThreshold = 1;
    cfg_ = c;
    clearUnhealthy();
  }

  // Drops queued frames. Cumulative drop counters stay.
  void clear() {
    count_ = 0;
    head_ = 0;
    openBatch_ = 1;
    for (uint8_t i = 0; i < kFamilySlots; ++i) fam_[i].used = 0;
  }

  void resetDrops() { drops_ = DropCounters{}; }

  void clearUnhealthy() {
    windowCount_ = 0;
    windowDrops_ = 0;
    windowHead_ = 0;
  }

  uint16_t count() const { return count_; }
  bool empty() const { return count_ == 0; }
  const DropCounters& drops() const { return drops_; }

  // ReplacedProtected is visible in drops() but excluded here and from the window.
  uint32_t unhealthyDropFrames() const {
    if (drops_.frames < drops_.byReplacedProtected) return 0;
    return drops_.frames - drops_.byReplacedProtected;
  }

  bool unhealthy() const {
    return windowCount_ > 0 && windowDrops_ >= cfg_.dropThreshold;
  }

  // Store one CRC-valid frame. Returns false when the incoming frame was not stored.
  bool push(const uint8_t* frame, size_t len, uint16_t messageType) {
    if (frame == nullptr || len == 0 || len > MaxFrame) {
      noteDrop(len > MaxFrame ? len : 0, DropReason::Oversize);
      sample(true);
      return false;
    }
    if (frameQueueIsProtected(messageType)) return pushProtected(frame, len, messageType);
    if (policyIsMsmWide(messageType)) return pushMsm(frame, len, messageType);
    return pushOther(frame, len, messageType);
  }

  // Copy the head frame when its length fits in writable. Otherwise leave it queued.
  size_t pull(uint8_t* dest, size_t writable) {
    if (dest == nullptr || count_ == 0) return 0;
    const Slot& s = slots_[head_];
    if (s.len == 0 || s.len > writable) return 0;
    for (uint16_t i = 0; i < s.len; ++i) dest[i] = s.data[i];
    const size_t n = s.len;
    head_ = static_cast<uint8_t>((static_cast<size_t>(head_) + 1u) % Depth);
    --count_;
    return n;
  }

  uint16_t headLength() const {
    if (count_ == 0) return 0;
    return slots_[head_].len;
  }

 private:
  struct Slot {
    uint16_t len = 0;
    uint16_t messageType = 0;
    uint32_t batchId = 0;
    uint8_t data[MaxFrame] = {};
  };
  struct Fam {
    uint16_t family = 0;
    uint32_t epoch = 0;
    uint8_t used = 0;
  };
  static constexpr uint8_t kFamilySlots = 8;

  Slot slots_[Depth];
  uint8_t head_ = 0;
  uint8_t count_ = 0;
  uint32_t openBatch_ = 1;
  Fam fam_[kFamilySlots] = {};
  DropCounters drops_{};
  FrameQueueConfig cfg_{};
  uint8_t window_[kWindowCap] = {};
  uint8_t windowHead_ = 0;
  uint8_t windowCount_ = 0;
  uint8_t windowDrops_ = 0;

  size_t indexOf(uint8_t logical) const {
    return (static_cast<size_t>(head_) + logical) % Depth;
  }

  void sample(bool drop) {
    const uint8_t n = cfg_.windowN == 0 ? static_cast<uint8_t>(kWindowCap) : cfg_.windowN;
    if (windowCount_ >= n) {
      if (window_[windowHead_]) {
        if (windowDrops_ > 0) --windowDrops_;
      }
      window_[windowHead_] = drop ? 1 : 0;
      windowHead_ = static_cast<uint8_t>((static_cast<unsigned>(windowHead_) + 1u) % n);
      if (drop) ++windowDrops_;
    } else {
      const uint8_t idx =
          static_cast<uint8_t>((static_cast<unsigned>(windowHead_) + windowCount_) % n);
      window_[idx] = drop ? 1 : 0;
      ++windowCount_;
      if (drop) ++windowDrops_;
    }
  }

  void noteDrop(size_t bytes, DropReason reason) {
    ++drops_.frames;
    drops_.bytes += static_cast<uint32_t>(bytes);
    switch (reason) {
      case DropReason::Oversize: ++drops_.byOversize; break;
      case DropReason::QueueFull: ++drops_.byQueueFull; break;
      case DropReason::MsmEpoch: ++drops_.byMsmEpoch; break;
      case DropReason::ProtectedKept: ++drops_.byProtectedKept; break;
      case DropReason::ReplacedProtected: ++drops_.byReplacedProtected; break;
      default: break;
    }
  }

  void copyInto(Slot& s, const uint8_t* frame, size_t len, uint16_t type, uint32_t batch) {
    s.len = static_cast<uint16_t>(len);
    s.messageType = type;
    s.batchId = batch;
    for (size_t i = 0; i < len; ++i) s.data[i] = frame[i];
  }

  void pushBack(const uint8_t* frame, size_t len, uint16_t type, uint32_t batch) {
    copyInto(slots_[indexOf(count_)], frame, len, type, batch);
    ++count_;
  }

  int findType(uint16_t type) const {
    for (uint8_t i = 0; i < count_; ++i) {
      if (slots_[indexOf(i)].messageType == type) return static_cast<int>(i);
    }
    return -1;
  }

  bool hasBatch(uint32_t batch) const {
    if (batch == 0) return false;
    for (uint8_t i = 0; i < count_; ++i) {
      if (slots_[indexOf(i)].batchId == batch) return true;
    }
    return false;
  }

  // onlyClosed: skip the open epoch so a multi-constellation second stays whole.
  uint32_t oldestMsmBatch(bool onlyClosed) const {
    for (uint8_t i = 0; i < count_; ++i) {
      const uint32_t b = slots_[indexOf(i)].batchId;
      if (b == 0) continue;
      if (onlyClosed && b == openBatch_) continue;
      return b;
    }
    return 0;
  }

  void removeAt(uint8_t logical) {
    for (uint8_t j = logical; static_cast<uint8_t>(j + 1) < count_; ++j) {
      slots_[indexOf(j)] = slots_[indexOf(static_cast<uint8_t>(j + 1))];
    }
    --count_;
  }

  uint16_t dropBatch(uint32_t batch) {
    if (batch == 0) return 0;
    uint16_t dropped = 0;
    uint8_t i = 0;
    while (i < count_) {
      Slot& s = slots_[indexOf(i)];
      if (s.batchId == batch) {
        noteDrop(s.len, DropReason::MsmEpoch);
        removeAt(i);
        ++dropped;
      } else {
        ++i;
      }
    }
    if (batch == openBatch_) {
      for (uint8_t f = 0; f < kFamilySlots; ++f) fam_[f].used = 0;
      ++openBatch_;
      if (openBatch_ == 0) openBatch_ = 1;
    }
    return dropped;
  }

  // Group MSM by observation epoch: a new DF004 for a constellation closes the
  // open batch so GPS+GLO+GAL+... of one second drop together.
  uint32_t assignBatch(uint16_t type, uint32_t epoch) {
    const uint16_t family = static_cast<uint16_t>(type / 10);
    int slot = -1;
    int freeSlot = -1;
    for (uint8_t i = 0; i < kFamilySlots; ++i) {
      if (!fam_[i].used) {
        if (freeSlot < 0) freeSlot = static_cast<int>(i);
        continue;
      }
      if (fam_[i].family == family) {
        slot = static_cast<int>(i);
        break;
      }
    }
    if (slot >= 0 && fam_[slot].epoch != epoch) {
      ++openBatch_;
      if (openBatch_ == 0) openBatch_ = 1;
      for (uint8_t i = 0; i < kFamilySlots; ++i) fam_[i].used = 0;
      slot = -1;
      freeSlot = 0;
    }
    if (slot < 0) {
      if (freeSlot < 0) {
        ++openBatch_;
        if (openBatch_ == 0) openBatch_ = 1;
        for (uint8_t i = 0; i < kFamilySlots; ++i) fam_[i].used = 0;
        freeSlot = 0;
      }
      fam_[freeSlot].used = 1;
      fam_[freeSlot].family = family;
      fam_[freeSlot].epoch = epoch;
    }
    return openBatch_;
  }

  bool pushProtected(const uint8_t* frame, size_t len, uint16_t messageType) {
    const int existing = findType(messageType);
    if (existing >= 0) {
      Slot& s = slots_[indexOf(static_cast<uint8_t>(existing))];
      noteDrop(s.len, DropReason::ReplacedProtected);
      copyInto(s, frame, len, messageType, 0);
      sample(false);
      return true;
    }
    if (count_ < Depth) {
      pushBack(frame, len, messageType, 0);
      sample(false);
      return true;
    }
    bool evicted = false;
    while (count_ >= Depth) {
      uint32_t b = oldestMsmBatch(true);
      if (b == 0) b = oldestMsmBatch(false);
      if (b == 0) break;
      dropBatch(b);
      evicted = true;
    }
    if (count_ < Depth) {
      pushBack(frame, len, messageType, 0);
      sample(evicted);
      return true;
    }
    noteDrop(len, DropReason::ProtectedKept);
    sample(true);
    return false;
  }

  bool pushMsm(const uint8_t* frame, size_t len, uint16_t messageType) {
    const uint32_t epoch = frameQueueMsmEpoch30(frame, len);
    const uint32_t batch = assignBatch(messageType, epoch);
    if (count_ < Depth) {
      pushBack(frame, len, messageType, batch);
      sample(false);
      return true;
    }
    const uint32_t older = oldestMsmBatch(true);
    if (older != 0) {
      dropBatch(older);
      if (count_ < Depth) {
        pushBack(frame, len, messageType, batch);
        sample(true);
        return true;
      }
    }
    if (hasBatch(batch)) {
      dropBatch(batch);
      noteDrop(len, DropReason::MsmEpoch);
      sample(true);
      return false;
    }
    noteDrop(len, DropReason::QueueFull);
    sample(true);
    return false;
  }

  bool pushOther(const uint8_t* frame, size_t len, uint16_t messageType) {
    if (count_ < Depth) {
      pushBack(frame, len, messageType, 0);
      sample(false);
      return true;
    }
    uint32_t b = oldestMsmBatch(true);
    if (b == 0) b = oldestMsmBatch(false);
    if (b != 0) {
      dropBatch(b);
      if (count_ < Depth) {
        pushBack(frame, len, messageType, 0);
        sample(true);
        return true;
      }
    }
    noteDrop(len, DropReason::QueueFull);
    sample(true);
    return false;
  }
};

}  // namespace tinyrtcm3
