#pragma once
#include <stdint.h>
#include <stddef.h>
#include "TinyRtcmTypes.h"

namespace tinyrtcm3 {

// =============================================================================
// CAP-REGISTRY â€” message-type dispatch (STUB)
// Requirements: REQ-REG-01, REQ-REG-02
// Intent: map DF002 message type to a handler without forcing apps into a
// giant switch. v0.1 provides the types and a no-op registry that always
// reports Unsupported so call sites can be written ahead of Codec work.
// =============================================================================

using MessageHandler = Status (*)(const uint8_t* frame, size_t len, void* ctx);

struct RegistryEntry {
  uint16_t messageType;
  MessageHandler handler;
};

class MessageRegistry {
 public:
  // Install a static table (not copied). STUB: stored but dispatch not used by Hub yet.
  void setTable(const RegistryEntry* table, size_t count) {
    table_ = table;
    count_ = count;
  }

  // Look up handler; returns Unsupported if missing (REQ-REG-02).
  Status dispatch(uint16_t messageType, const uint8_t* frame, size_t len, void* ctx) const {
    if (table_ == nullptr) return Status::Unsupported;
    for (size_t i = 0; i < count_; ++i) {
      if (table_[i].messageType == messageType && table_[i].handler != nullptr) {
        return table_[i].handler(frame, len, ctx);
      }
    }
    return Status::Unsupported;
  }

 private:
  const RegistryEntry* table_ = nullptr;
  size_t count_ = 0;
};

}  // namespace tinyrtcm3

