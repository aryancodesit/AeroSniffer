#pragma once
#ifndef MEMORY_ENGINE_H
#define MEMORY_ENGINE_H

#include <stdint.h>
#include "MemoryTypes.h"

class MemoryEngineClass {
public:
  void begin();
  void tick();
  MemorySummary recall();
  void flush();
  void onTouchEvent(uint16_t duration_ms = 0);
  uint16_t touchEventCount() const { return touch_events_; }

private:
  void observe();
  void decay();
  void prune();
  bool try_dedup(uint8_t domain, uint8_t subtype, uint8_t salience);
  uint8_t compute_salience(uint8_t base);
  uint8_t evict_one();

  MemoryRecord records_[MEMORY_MAX_RECORDS];
  uint16_t     record_count_;
  uint16_t     head_;
  uint32_t     last_decay_ms_;
  bool         initialized_;

  uint32_t     tap_timestamps_[8];
  uint8_t      tap_index_;
  uint32_t     double_tap_last_ms_;
  uint32_t     burst_window_start_;
  uint8_t      burst_count_;
  bool         burst_armed_;

  uint16_t     touch_taps_session_;
  uint16_t     touch_events_;

  uint16_t     decay_acc_[DOMAIN_COUNT];

  uint32_t     boot_count_;
  uint32_t     sequence_;
};

extern MemoryEngineClass MemoryEngine;

#endif
