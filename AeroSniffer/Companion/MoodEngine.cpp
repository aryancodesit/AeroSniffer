// ================================================================
//  Companion/MoodEngine.cpp  —  Slow Behavioral Observer (V2.5)
//  AeroSniffer Companion Layer | C++ Implementation
// ================================================================
#include "Companion/MoodEngine.h"

// ── Lifecycle ────────────────────────────────────────────────────

void MoodEngineClass::begin() {
  _mood              = MOOD_RELAXED;
  _strength          = 50;
  _last_mood_change  = millis();
  _focus_accumulator = 0;
  _playful_condition_met_since = 0;

  g_creature.mood          = MOOD_RELAXED;
  g_creature.mood_strength = 50;

  Serial.println("[MOOD] begin -> RELAXED");
}

void MoodEngineClass::tick(uint32_t delta_ms) {
  (void)delta_ms;
  uint32_t now = millis();
  MoodType old_mood = _mood;

  // ── 1. Decay current mood strength ───────────────────────────
  if (_mood != MOOD_RELAXED && _strength > 0) {
    uint32_t elapsed = now - _last_mood_change;
    uint32_t interval = decayIntervalMs(_mood);
    uint8_t decayed = elapsed / interval;
    if (decayed > _strength) {
      _strength = 0;
    } else {
      _strength -= decayed;
    }
  }

  // ── 2. Track hold-off conditions ─────────────────────────────
  // PLAYFUL hold-off: entry condition must be met continuously for 30s
  if (playfulEntryCondition()) {
    if (_playful_condition_met_since == 0) {
      _playful_condition_met_since = now;
    }
  } else {
    _playful_condition_met_since = 0;
  }

  // ── 3. Resolve next mood (arbitration) ───────────────────────
  MoodType next = resolveNextMood();

  // ── 4. Transition if needed ──────────────────────────────────
  if (next != _mood) {
    _mood             = next;
    _strength         = 50;
    _last_mood_change = now;
    Serial.printf("[MOOD] %s -> %s\n", moodName(old_mood), moodName(next));
  }

  // ── 5. Publish to CreatureState ──────────────────────────────
  publish();
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

  // P4 — PLAYFUL: requires 30s hold-off for upward transition
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
  return g_creature.attention.source == SOURCE_SECURITY;
}

bool MoodEngineClass::playfulEntryCondition() const {
  EmotionType e = g_creature.emotion;
  return (e == EMOTION_HAPPY || e == EMOTION_LOVE ||
          e == EMOTION_EXCITED || e == EMOTION_SURPRISED);
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
