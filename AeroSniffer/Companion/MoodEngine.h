// ================================================================
//  Companion/MoodEngine.h  —  Slow Behavioral Observer
//  AeroSniffer Companion Layer | C++ Class
// ================================================================
#pragma once

#include <Arduino.h>
#include "AeroSnifferOS.h"

// ── Mood Engine ─────────────────────────────────────────────────
// Slow observer that reads CreatureState (emotion, attention, uptime)
// and derives mood as a long-lived behavioral layer.
//
// Timescale: Mood operates on hours, not seconds.
// Input: reads g_creature.{emotion, attention, interactions_today}
// Output: writes g_creature.{mood, mood_strength}
//
// Thread model:
//   tick() — called from Core 1 render loop only.
//   No EventBus subscriptions, no spinlocks, no cross-core writes.

class MoodEngineClass {
public:
  void begin();
  void tick(uint32_t delta_ms);
  void publish();
  static const char* moodName(MoodType m);

private:
  MoodType  _mood    = MOOD_RELAXED;
  uint8_t   _strength = 50;
  uint32_t  _last_mood_change = 0;
  uint32_t  _focus_accumulator = 0;

  // Hold-off tracking for upward transitions
  uint32_t  _playful_condition_met_since = 0;

  // Touch history ring buffer (sliding 5-min window for PLAYFUL)
  uint32_t  _touch_timestamps[10];
  uint8_t   _touch_count;
  bool      _prev_was_touch_source;

  // Positive emotion recency (PLAYFUL requires positive emotion within ~60s)
  uint32_t  _last_positive_emotion = 0;

  static uint32_t decayIntervalMs(MoodType m);
  MoodType resolveNextMood();
  bool playfulEntryCondition() const;
  bool anxiousEntryCondition() const;
  void recordTouch(uint32_t now);
  void pruneOldTouches(uint32_t now);
};
