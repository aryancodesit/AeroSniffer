#pragma once
#ifndef MEMORY_ENGINE_H
#define MEMORY_ENGINE_H

#include <stdint.h>
#include "MemoryTypes.h"
#include "AeroSnifferOS.h"

class MemoryEngineClass {
public:
  void begin();
  void tick();
  MemorySummary recall();
  void flush();
  void onTouchEvent(uint16_t duration_ms = 0);
  void onSecurityEvent(EventType event, void* data = nullptr);
  void onFlightEvent(EventType event, void* data = nullptr);
  uint16_t touchEventCount() const { return pending_head_ - pending_tail_; }

private:
  void observe();
  void decay();
  void prune();
  bool try_dedup(uint8_t domain, uint8_t subtype, uint8_t salience, uint32_t source_id = 0);
  uint8_t compute_salience(uint8_t base);
  uint8_t evict_one();

  void observe_security(uint8_t subtype, uint8_t event_count, uint8_t threat_level, uint32_t source_id);
  void observe_aviation(uint8_t subtype, uint32_t icao24, int16_t alt, uint8_t rare);
  void observe_mood(uint8_t subtype, uint8_t mood, uint8_t prev_mood, uint16_t duration_min);

  struct {
    uint8_t  pending_subtype;
    uint8_t  pending_count;
    uint8_t  pending_threat_level;
    uint32_t deauth_window_start;
    uint8_t  deauth_count;
    bool     suspicious_formed;
  } sec_;

  struct {
    uint32_t last_flight_ms;
    uint32_t last_quiet_sky_ms;
    bool     flight_pending;
    bool     pending_is_rare;
  } avi_;

  struct {
    uint8_t  last_known_mood;
    uint32_t current_since_ms;
    uint8_t  last_period_type;
  } mood_;

  MemoryRecord records_[MEMORY_MAX_RECORDS];
  uint16_t     record_count_;
  uint16_t     head_;
  uint32_t     last_decay_ms_;
  bool         initialized_;

  uint32_t     tap_timestamps_[8];
  uint8_t      tap_index_;
  bool         burst_armed_;

  uint16_t     touch_taps_session_;

  struct PendingTouch {
    uint16_t duration_ms;
  };
  PendingTouch pending_[8];
  uint16_t     pending_head_;
  uint16_t     pending_tail_;
  uint16_t     dropped_touch_events_;

  uint16_t     decay_acc_[DOMAIN_COUNT];

  uint32_t     boot_count_;
  uint32_t     sequence_;
};

extern MemoryEngineClass MemoryEngine;

#endif
