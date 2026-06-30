// ================================================================
//  MoodEngine.cpp  —  Slow Behavioral Observer (V2.5)
//  AeroSniffer Companion Layer | C++ Implementation
// ================================================================
#include "Companion/MoodEngine.h"

// ── Lifecycle ────────────────────────────────────────────────────

void MoodEngineClass::begin() {
  _mood              = MOOD_RELAXED;
  _strength          = 50;
  _last_mood_transition = millis();
  _last_decay_tick      = millis();
  _focus_accumulator = 0;
  _playful_condition_met_since = 0;
  _touch_count       = 0;
  _prev_was_touch_source = false;
  _last_positive_emotion = 0;

  g_creature.mood          = MOOD_RELAXED;
  g_creature.mood_strength = 50;

  Serial.println("[MOOD] begin -> RELAXED");
}

void MoodEngineClass::tick(uint32_t delta_ms) {
  (void)delta_ms;
  uint32_t now = millis();
  MoodType old_mood = _mood;

  // ── 1. Track touch events via attention.source transitions ──
  // Detect SOURCE_TOUCH rising edge: attention.source changed to
  // SOURCE_TOUCH since last tick.  This avoids EventBus access
  // while still detecting each distinct touch.
  if (g_creature.attention.source == SOURCE_TOUCH && !_prev_was_touch_source) {
    recordTouch(now);
#ifdef CALIBRATION
    CALIB("ts_touch_mood", micros());
#endif
  }
  _prev_was_touch_source = (g_creature.attention.source == SOURCE_TOUCH);
  pruneOldTouches(now);

  // ── 2. Track positive emotion recency ─────────────────────────
  // Record last time any positive emotion was seen.  PLAYFUL uses a
  // 60-second recency window so the 30s hold-off can complete even
  // though individual emotions only last ~5s before decaying to IDLE.
  switch (g_creature.emotion) {
    case EMOTION_HAPPY:
    case EMOTION_EXCITED:
    case EMOTION_LOVE:
    case EMOTION_SURPRISED:
      _last_positive_emotion = now;
      break;
    default:
      break;
  }

  // ── 3. Decay current mood strength ───────────────────────────
  if (_mood != MOOD_RELAXED && _strength > 0) {
    uint32_t elapsed = now - _last_decay_tick;
    uint32_t interval = decayIntervalMs(_mood);
    if (elapsed >= interval) {
      uint8_t steps = elapsed / interval;
      if (steps > _strength) {
        _strength = 0;
      } else {
        _strength -= steps;
      }
      _last_decay_tick += steps * interval;
    }
  }

  // ── 4. Track hold-off conditions ─────────────────────────────
  // PLAYFUL hold-off: entry condition must be met continuously for 30s.
  // The condition uses _touch_count + positive emotion recency (60s
  // window) — not instantaneous emotion — so the ~5s emotion lifetime
  // does not reset the hold-off.
  if (playfulEntryCondition()) {
    if (_playful_condition_met_since == 0) {
      _playful_condition_met_since = now;
    }
  } else {
    _playful_condition_met_since = 0;
  }

  // ── 5. Resolve next mood (arbitration) ───────────────────────
  MoodType next = resolveNextMood();

  // ── 6. Transition if needed ──────────────────────────────────
  if (next != _mood) {
    _mood             = next;
    _strength         = 50;
    _last_mood_transition = now;
    _last_decay_tick      = now;
    _playful_condition_met_since = 0;
    Serial.printf("[MOOD] %s -> %s\n", moodName(old_mood), moodName(next));
  }

  // ── 7. Publish to CreatureState ──────────────────────────────
  publish();

#ifdef CALIBRATION
  CALIB_RATE("mood_str", _strength, 1000);
#endif
}

// ── Touch History ─────────────────────────────────────────────────

void MoodEngineClass::recordTouch(uint32_t now) {
  if (_touch_count < 10) {
    _touch_timestamps[_touch_count++] = now;
  }
}

void MoodEngineClass::pruneOldTouches(uint32_t now) {
  uint8_t write_idx = 0;
  for (uint8_t i = 0; i < _touch_count; i++) {
    if ((now - _touch_timestamps[i]) <= 300000) {
      _touch_timestamps[write_idx++] = _touch_timestamps[i];
    }
  }
  _touch_count = write_idx;
}

// ── Arbitration (Fixed Priority) ─────────────────────────────────
//
// Priority: ANXIOUS (P1) > PLAYFUL (P4) > RELAXED (P7)
// ANXIOUS bypasses hold-off.  PLAYFUL requires 30s hold-off.
// RELAXED is the default when no other mood qualifies.
MoodType MoodEngineClass::resolveNextMood() {
  uint32_t now = millis();

  // P1 — ANXIOUS: immediate, bypasses hold-off
  if (anxiousEntryCondition()) {
    return MOOD_ANXIOUS;
  }

  // P4 — PLAYFUL: ≥2 touches in 5 min, positive emotion within 60s,
  //               and 30s hold-off for upward transition
  if (_mood == MOOD_RELAXED || _mood == MOOD_PLAYFUL) {
    if (playfulEntryCondition()) {
      if (_playful_condition_met_since > 0 &&
          (now - _playful_condition_met_since >= 30000)) {
        return MOOD_PLAYFUL;
      }
    }
  }

  // If current mood still has strength, keep it
  if (_strength > 0) {
    return _mood;
  }

  // P7 — RELAXED: default
  return MOOD_RELAXED;
}

// ── Entry Conditions ─────────────────────────────────────────────

bool MoodEngineClass::anxiousEntryCondition() const {
  return g_creature.attention.target == TARGET_THREAT &&
         g_creature.emotion == EMOTION_ALERT;
}

bool MoodEngineClass::playfulEntryCondition() const {
  uint32_t now = millis();
  return _touch_count >= 2 &&
         (now - _last_positive_emotion < 60000);
}

// ── Publish ───────────────────────────────────────────────────────

void MoodEngineClass::publish() {
  g_creature.mood          = _mood;
  g_creature.mood_strength = _strength;
}

// ── Helpers ───────────────────────────────────────────────────────

const char* MoodEngineClass::moodName(MoodType m) {
  switch (m) {
    case MOOD_RELAXED: return "RELAXED";
    case MOOD_PLAYFUL: return "PLAYFUL";
    case MOOD_ANXIOUS: return "ANXIOUS";
    default:           return "?";
  }
}

uint32_t MoodEngineClass::decayIntervalMs(MoodType m) {
  switch (m) {
    case MOOD_PLAYFUL: return 36000;   // 36s per unit → 50 units = 30 min
    case MOOD_ANXIOUS: return 24000;   // 24s per unit → 50 units = 20 min
    default:           return 3600000; // 1h (shouldn't be used — RELAXED doesn't decay)
  }
}
