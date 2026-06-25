#include "Memory/MemoryEngine.h"
#include "AeroSnifferOS.h"
#include "Config.h"

MemoryEngineClass MemoryEngine;

// ── Accumulator-based half-life decay ─────────────────────────
// Each tick (60s), add DECAY_RATE_100[d] to a per-domain accumulator.
// When accumulator >= 100, decrement all records of that domain by 1.
// Rate = round(5000 / half_life_ticks) → yields ~50 decrements per half-life
// Half-life ticks: TOUCH=360, SECURITY=720, AVIATION=240, MOOD=480, ATTN=180
static constexpr uint16_t DECAY_RATE_100[DOMAIN_COUNT] = {
  14,   // DOMAIN_TOUCH:     5000/360  = 13.9 → 14
  7,    // DOMAIN_SECURITY:  5000/720  =  6.9 →  7
  21,   // DOMAIN_AVIATION:  5000/240  = 20.8 → 21
  10,   // DOMAIN_MOOD:      5000/480  = 10.4 → 10
  28    // DOMAIN_ATTENTION: 5000/180  = 27.8 → 28
};

void MemoryEngineClass::begin() {
  record_count_ = 0;
  head_ = 0;
  last_decay_ms_ = 0;
  initialized_ = false;

  tap_index_ = 0;
  burst_armed_ = false;
  touch_taps_session_ = 0;

  pending_head_ = 0;
  pending_tail_ = 0;
  dropped_touch_events_ = 0;

  for (uint8_t d = 0; d < DOMAIN_COUNT; d++) {
    decay_acc_[d] = 0;
  }

  for (uint16_t i = 0; i < MEMORY_MAX_RECORDS; i++) {
    records_[i].id = 0;
    records_[i].strength = 0;
  }

  // Derive boot_count from CreatureProfile's boot counter
  // PersistenceService stores boot_count; read it from NVS directly via StorageService
  boot_count_ = StorageService.getStat("boot_count", 0);
  sequence_ = 0;

  // ── Security domain state ──
  sec_.pending_subtype = 0;
  sec_.pending_count = 0;
  sec_.pending_threat_level = 0;
  sec_.deauth_window_start = millis();
  sec_.deauth_count = 0;
  sec_.suspicious_formed = false;

  // ── Aviation domain state ──
  avi_.last_flight_ms = millis();
  avi_.last_quiet_sky_ms = 0;
  avi_.flight_pending = false;
  avi_.pending_is_rare = false;

  // ── Mood domain state ──
  mood_.last_known_mood = (uint8_t)g_creature.mood;
  mood_.current_since_ms = millis();
  mood_.last_period_type = 0xFF;

  initialized_ = true;
  Serial.printf("[MEM] begin -> %d records (boot=%u)\n", record_count_, boot_count_);
}

void MemoryEngineClass::tick() {
  if (!initialized_) return;

  // Defensive: clamp record_count_ to array bounds
  if (record_count_ > MEMORY_MAX_RECORDS) {
    record_count_ = MEMORY_MAX_RECORDS;
  }
  if (head_ >= MEMORY_MAX_RECORDS) {
    head_ = 0;
  }

  observe();

  uint32_t now = millis();
  if (now - last_decay_ms_ >= 60000) {
    last_decay_ms_ = now;
    decay();
    prune();

    if (record_count_ > 0) {
      Serial.printf("[MEM] tick: records=%d strength=%d\n",
        record_count_, recall().domain_strength[DOMAIN_TOUCH]);
    }
  }
}

void MemoryEngineClass::onTouchEvent(uint16_t duration_ms) {
  PendingTouch& pt = pending_[pending_head_ % 8];
  pt.duration_ms = duration_ms;
  pending_head_++;
  if (pending_head_ - pending_tail_ > 8) {
    pending_tail_ = pending_head_ - 8;
    dropped_touch_events_++;
  }
}

// ── Security event callback (runs on Core 0 — sets flags only) ────
void MemoryEngineClass::onSecurityEvent(EventType event, void* data) {
  (void)data;
  uint32_t now = millis();

  switch (event) {
    case EVENT_ATTACK_DEAUTH: {
      // Reset window if expired
      if (now - sec_.deauth_window_start > 60000) {
        sec_.deauth_window_start = now;
        sec_.deauth_count = 0;
        sec_.suspicious_formed = false;
      }
      sec_.deauth_count++;

      // Evaluate thresholds — set pending flags, do NOT form records
      if (sec_.deauth_count >= 5) {
        sec_.pending_threat_level = (sec_.deauth_count * 20 > 100) ? 100 : (uint8_t)(sec_.deauth_count * 20);
        sec_.pending_count = sec_.deauth_count;
        sec_.pending_subtype = SEC_THREAT_DETECTED;
        __sync_synchronize();
        // Reset window — burst escalated to threat level
        sec_.deauth_window_start = now;
        sec_.deauth_count = 0;
        sec_.suspicious_formed = false;
      } else if (sec_.deauth_count >= 2 && !sec_.suspicious_formed) {
        sec_.pending_threat_level = (uint8_t)(sec_.deauth_count * 20);
        sec_.pending_count = sec_.deauth_count;
        sec_.suspicious_formed = true;
        sec_.pending_subtype = SEC_SUSPICIOUS_ACTIVITY;
        __sync_synchronize();
        // Do NOT reset window — continue counting toward THREAT
      }
      break;
    }
    case EVENT_ATTACK_EVILTWIN: {
      sec_.pending_threat_level = 90;
      sec_.pending_count = 1;
      sec_.pending_subtype = SEC_THREAT_DETECTED;
      __sync_synchronize();
      break;
    }
    case EVENT_DEVICE_TRUSTED:
    case EVENT_DEVICE_FAMILIAR:
    case EVENT_DEVICE_UNKNOWN: {
      sec_.pending_threat_level = 10;
      sec_.pending_count = 1;
      sec_.pending_subtype = SEC_DEVICE_APPEARED;
      __sync_synchronize();
      break;
    }
    default:
      break;
  }
}

// ── Aviation event callback (runs on Core 0 — sets flags only) ────
void MemoryEngineClass::onFlightEvent(EventType event, void* data) {
  uint32_t now = millis();
  avi_.last_flight_ms = now;

  switch (event) {
    case EVENT_FLIGHT_DETECTED: {
      avi_.flight_pending = true;
      avi_.pending_is_rare = false;
      break;
    }
    case EVENT_FLIGHT_RARE: {
      avi_.flight_pending = true;
      avi_.pending_is_rare = true;
      break;
    }
    default:
      break;
  }
}

void MemoryEngineClass::observe() {
  uint32_t now = millis();

  if (record_count_ > MEMORY_MAX_RECORDS) {
    record_count_ = MEMORY_MAX_RECORDS;
  }

  // ── TOUCH: drain pending buffer (unchanged from Sprint 2) ──
  while (pending_tail_ < pending_head_) {
    PendingTouch& pt = pending_[pending_tail_ % 8];
    uint16_t duration_ms = pt.duration_ms;
    pending_tail_++;

    // Record tap timestamp in sliding window
    tap_timestamps_[tap_index_ % 8] = now;
    tap_index_++;
    touch_taps_session_++;

    // ── Duration-based classification ──────────────────────
    uint8_t subtype;
    uint8_t salience_base;

    if (duration_ms <= TAP_MAX_MS) {
      subtype = TOUCH_TAP;
      salience_base = 22;
    } else if (duration_ms <= HOLD_MAX_MS) {
      subtype = TOUCH_HOLD;
      salience_base = constrain(duration_ms / 20, 22, 35);
    } else {
      subtype = TOUCH_LONG_HOLD;
      salience_base = constrain(duration_ms / 30, 30, 45);
    }

    // ── Double-tap detection (promotes to DOUBLE) ──────────
    if (tap_index_ >= 2) {
      uint32_t gap = tap_timestamps_[(tap_index_ - 1) % 8]
                   - tap_timestamps_[(tap_index_ - 2) % 8];
      if (gap > 50 && gap <= 500) {
        subtype = TOUCH_DOUBLE;
        salience_base = 30;
      }
    }

    // ── Burst window count ────────────────────────────────
    uint8_t count_10s = 0;
    for (uint8_t i = 0; i < 8 && i < tap_index_; i++) {
      if (now - tap_timestamps_[i] <= 10000) count_10s++;
    }

    uint8_t sal = compute_salience(salience_base);

    // ── Form memory record ───────────────────────────────
    if (!try_dedup(DOMAIN_TOUCH, subtype, sal)) {
      MemoryRecord rec = {};
      rec.id = (boot_count_ << 24) | (sequence_++);
      rec.formed_at_ms = now;
      rec.category = MEM_CAT_EPISODIC;
      rec.domain = DOMAIN_TOUCH;
      rec.subtype = subtype;
      rec.salience = sal;
      rec.strength = 100;
      rec.mood_at_formation = g_creature.mood;
      rec.attention_at_form = g_creature.attention.target;
      rec.mode_at_formation = g_creature.activity;
      rec.data.touch.touch_duration_ms = duration_ms;
      rec.data.touch.tap_count_window = count_10s;

      if (record_count_ >= MEMORY_MAX_RECORDS) evict_one();
      records_[head_] = rec;
      head_ = (head_ + 1) % MEMORY_MAX_RECORDS;
      if (record_count_ < MEMORY_MAX_RECORDS) record_count_++;


    }

    // ── Burst record (additive, fires once per session) ────
    if (count_10s >= 5 && !burst_armed_) {
      burst_armed_ = true;
      uint8_t burst_sal = compute_salience(45);
      if (!try_dedup(DOMAIN_TOUCH, TOUCH_BURST, burst_sal)) {
        MemoryRecord rec = {};
        rec.id = (boot_count_ << 24) | (sequence_++);
        rec.formed_at_ms = now;
        rec.category = MEM_CAT_EPISODIC;
        rec.domain = DOMAIN_TOUCH;
        rec.subtype = TOUCH_BURST;
        rec.salience = burst_sal;
        rec.strength = 100;
        rec.mood_at_formation = g_creature.mood;
        rec.attention_at_form = g_creature.attention.target;
        rec.mode_at_formation = g_creature.activity;
        rec.data.touch.touch_duration_ms = duration_ms;
        rec.data.touch.tap_count_window = count_10s;

        if (record_count_ >= MEMORY_MAX_RECORDS) evict_one();
        records_[head_] = rec;
        head_ = (head_ + 1) % MEMORY_MAX_RECORDS;
        if (record_count_ < MEMORY_MAX_RECORDS) record_count_++;


      }
    }
  }

  // ── Reset burst_armed_ when 10s window has no recent taps ──
  if (burst_armed_) {
    bool recent = false;
    for (uint8_t i = 0; i < 8 && i < tap_index_; i++) {
      if (now - tap_timestamps_[i] <= 10000) { recent = true; break; }
    }
    if (!recent) burst_armed_ = false;
  }

  // ── SECURITY: consume pending flags (set by onSecurityEvent) ──
  __sync_synchronize();
  if (sec_.pending_subtype != 0) {
    uint8_t st = sec_.pending_subtype;
    uint8_t cnt = sec_.pending_count;
    uint8_t tl = sec_.pending_threat_level;
    sec_.pending_subtype = 0;
    __sync_synchronize();
    observe_security(st, cnt, tl, 0);
  }

  // ── AVIATION: consume pending flight events ──
  if (avi_.flight_pending) {
    avi_.flight_pending = false;
    uint8_t avi_subtype = avi_.pending_is_rare ? AVI_UNUSUAL_TRAFFIC_EVENT : AVI_AIRCRAFT_SPOTTED;
    observe_aviation(avi_subtype, 0, -1, avi_.pending_is_rare ? 1 : 0);
  }

  // ── AVIATION: quiet sky check (time-based) ──
  if (avi_.last_flight_ms > 0 && now - avi_.last_flight_ms >= 1800000) {
    if (now - avi_.last_quiet_sky_ms >= 3600000) {
      observe_aviation(AVI_QUIET_SKY_PERIOD, 0, -1, 0);
      avi_.last_quiet_sky_ms = now;
    }
  }

  // ── MOOD: poll g_creature.mood ──
  uint8_t current_mood = (uint8_t)g_creature.mood;

  // 1. Mood transition detection (never deduped)
  if (current_mood != mood_.last_known_mood) {
    uint16_t duration = (now - mood_.current_since_ms) / 60000;
    observe_mood(MOOD_TRANSITION, current_mood, mood_.last_known_mood, duration);

    mood_.last_known_mood = current_mood;
    mood_.current_since_ms = now;
    mood_.last_period_type = 0xFF;
  }

  // 2. Prolonged mood check (30+ min in same mood)
  uint32_t elapsed_min = (now - mood_.current_since_ms) / 60000;
  if (elapsed_min >= 30 && mood_.last_period_type != current_mood) {
    uint8_t new_subtype = 0xFF;
    switch (current_mood) {
      case MOOD_RELAXED: new_subtype = MOOD_LONG_RELAXED_PERIOD; break;
      case MOOD_PLAYFUL: new_subtype = MOOD_LONG_PLAYFUL_PERIOD; break;
      case MOOD_ANXIOUS: new_subtype = MOOD_LONG_ANXIOUS_PERIOD; break;
    }
    if (new_subtype != 0xFF) {
      observe_mood(new_subtype, current_mood, 0xFF, (uint16_t)elapsed_min);
      mood_.last_period_type = current_mood;
    }
  }
}

uint8_t MemoryEngineClass::compute_salience(uint8_t base) {
  float ctx = 1.0f;

  // Mode-based multiplier
  if (g_creature.activity == ACTIVITY_IDLE) ctx *= 1.1f;

  // Mood context
  if (g_creature.mood == MOOD_ANXIOUS) ctx *= 1.2f;
  if (g_creature.mood == MOOD_PLAYFUL) ctx *= 0.8f;

  // Mood intensity
  if (g_creature.mood_strength > 70) ctx *= 1.2f;
  if (g_creature.mood_strength < 20) ctx *= 0.7f;

  uint8_t result = (uint8_t)(base * ctx + 0.5f);
  if (result > 100) result = 100;
  if (result < 1)   result = 1;
  return result;
}

// ── try_dedup: domain-specific windows with source_id matching ────
bool MemoryEngineClass::try_dedup(uint8_t domain, uint8_t subtype, uint8_t salience, uint32_t source_id) {
  uint32_t now = millis();

  uint32_t window_ms;
  switch (domain) {
    case DOMAIN_TOUCH:    window_ms = 60000;   break;
    case DOMAIN_SECURITY: window_ms = 300000;  break;
    case DOMAIN_AVIATION: window_ms = 180000;  break;
    case DOMAIN_MOOD:
      window_ms = (subtype == MOOD_TRANSITION) ? 0 : 300000;
      break;
    default:              window_ms = 60000;   break;
  }
  if (window_ms == 0) return false;

  for (uint16_t i = 0; i < record_count_; i++) {
    auto& rec = records_[i];
    if (rec.domain == domain && rec.subtype == subtype
        && rec.category == MEM_CAT_EPISODIC
        && (now - rec.formed_at_ms) <= window_ms
        && rec.strength > 50
        && rec.source_id == source_id) {

      uint8_t new_sal = rec.salience + 10;
      if (new_sal > 100) new_sal = 100;
      rec.salience = new_sal;
      rec.strength = 100;

      // MOOD safety-net: never extend the dedup window
      if (domain != DOMAIN_MOOD) {
        rec.formed_at_ms = now;
      }


      return true;
    }
  }
  return false;
}

// ── Security record formation ─────────────────────────────────────
void MemoryEngineClass::observe_security(uint8_t subtype, uint8_t event_count, uint8_t threat_level, uint32_t source_id) {
  uint32_t now = millis();
  uint8_t base_salience;
  switch (subtype) {
    case SEC_DEVICE_APPEARED:    base_salience = 25; break;
    case SEC_SUSPICIOUS_ACTIVITY: base_salience = 38; break;
    case SEC_THREAT_DETECTED:     base_salience = 50; break;
    default: return;
  }
  uint8_t sal = compute_salience(base_salience);

  if (!try_dedup(DOMAIN_SECURITY, subtype, sal, source_id)) {
    MemoryRecord rec = {};
    rec.id = (boot_count_ << 24) | (sequence_++);
    rec.source_id = source_id;
    rec.formed_at_ms = now;
    rec.category = MEM_CAT_EPISODIC;
    rec.domain = DOMAIN_SECURITY;
    rec.subtype = subtype;
    rec.salience = sal;
    rec.strength = 100;
    rec.mood_at_formation = g_creature.mood;
    rec.attention_at_form = g_creature.attention.target;
    rec.mode_at_formation = g_creature.activity;
    rec.data.security.threat_level = threat_level;
    rec.data.security.event_count = event_count;

    if (record_count_ >= MEMORY_MAX_RECORDS) evict_one();
    records_[head_] = rec;
    head_ = (head_ + 1) % MEMORY_MAX_RECORDS;
    if (record_count_ < MEMORY_MAX_RECORDS) record_count_++;


  }
}

// ── Aviation record formation ────────────────────────────────────
void MemoryEngineClass::observe_aviation(uint8_t subtype, uint32_t icao24, int16_t alt, uint8_t rare) {
  uint32_t now = millis();
  uint8_t base_salience;
  switch (subtype) {
    case AVI_AIRCRAFT_SPOTTED:      base_salience = 22; break;
    case AVI_QUIET_SKY_PERIOD:      base_salience = 16; break;
    case AVI_UNUSUAL_TRAFFIC_EVENT: base_salience = 42; break;
    default: return;
  }
  uint8_t sal = compute_salience(base_salience);
  uint32_t source_id = icao24;

  if (!try_dedup(DOMAIN_AVIATION, subtype, sal, source_id)) {
    MemoryRecord rec = {};
    rec.id = (boot_count_ << 24) | (sequence_++);
    rec.source_id = source_id;
    rec.formed_at_ms = now;
    rec.category = MEM_CAT_EPISODIC;
    rec.domain = DOMAIN_AVIATION;
    rec.subtype = subtype;
    rec.salience = sal;
    rec.strength = 100;
    rec.mood_at_formation = g_creature.mood;
    rec.attention_at_form = g_creature.attention.target;
    rec.mode_at_formation = g_creature.activity;
    rec.data.aviation.icao24 = icao24;
    rec.data.aviation.altitude_ft = alt;
    rec.data.aviation.is_rare = rare;

    if (record_count_ >= MEMORY_MAX_RECORDS) evict_one();
    records_[head_] = rec;
    head_ = (head_ + 1) % MEMORY_MAX_RECORDS;
    if (record_count_ < MEMORY_MAX_RECORDS) record_count_++;


  }
}

// ── Mood record formation ────────────────────────────────────────
void MemoryEngineClass::observe_mood(uint8_t subtype, uint8_t mood, uint8_t prev_mood, uint16_t duration_min) {
  uint32_t now = millis();
  uint8_t base_salience;
  switch (subtype) {
    case MOOD_LONG_RELAXED_PERIOD: base_salience = 25; break;
    case MOOD_LONG_PLAYFUL_PERIOD: base_salience = 28; break;
    case MOOD_LONG_ANXIOUS_PERIOD: base_salience = 35; break;
    case MOOD_TRANSITION:          base_salience = 30; break;
    default: return;
  }
  uint8_t sal = compute_salience(base_salience);

  // MOOD_TRANSITION: never deduped (skip try_dedup entirely)
  // MOOD_LONG_*: try_dedup with 300s safety-net window
  bool should_form = (subtype == MOOD_TRANSITION)
    ? true
    : !try_dedup(DOMAIN_MOOD, subtype, sal, 0);

  if (should_form) {
    MemoryRecord rec = {};
    rec.id = (boot_count_ << 24) | (sequence_++);
    rec.source_id = 0;
    rec.formed_at_ms = now;
    rec.category = MEM_CAT_EPISODIC;
    rec.domain = DOMAIN_MOOD;
    rec.subtype = subtype;
    rec.salience = sal;
    rec.strength = 100;
    rec.mood_at_formation = g_creature.mood;
    rec.attention_at_form = g_creature.attention.target;
    rec.mode_at_formation = g_creature.activity;
    rec.data.mood_event.mood = mood;
    rec.data.mood_event.prev_mood = prev_mood;
    rec.data.mood_event.duration_min = duration_min;

    if (record_count_ >= MEMORY_MAX_RECORDS) evict_one();
    records_[head_] = rec;
    head_ = (head_ + 1) % MEMORY_MAX_RECORDS;
    if (record_count_ < MEMORY_MAX_RECORDS) record_count_++;


  }
}

// ── Core engine pipeline (unchanged from Sprint 2) ───────────────

void MemoryEngineClass::decay() {
  // Advance accumulators and count pending decrements per domain
  uint8_t pending[DOMAIN_COUNT] = {0};

  for (uint8_t d = 0; d < DOMAIN_COUNT; d++) {
    decay_acc_[d] += DECAY_RATE_100[d];
    while (decay_acc_[d] >= 100) {
      decay_acc_[d] -= 100;
      pending[d]++;
    }
  }

  // Apply pending decrements — one pass over active records
  for (uint16_t i = 0; i < record_count_; i++) {
    auto& rec = records_[i];
    if (rec.strength == 0) continue;
    uint8_t d = rec.domain;
    if (d >= DOMAIN_COUNT) {
      rec.strength = 0;
      continue;
    }
    uint8_t p = pending[d];
    if (p > 0) {
      rec.strength = (rec.strength > p) ? (rec.strength - p) : 0;
    }
  }
}

void MemoryEngineClass::prune() {
  uint16_t write = 0;
  for (uint16_t read = 0; read < record_count_; read++) {
    if (records_[read].strength > 0) {
      if (write != read) {
        records_[write] = records_[read];
      }
      write++;
    }
  }
  record_count_ = write;
  head_ = record_count_;
}

uint8_t MemoryEngineClass::evict_one() {
  uint16_t best = 0;
  uint8_t lowest = 255;

  for (uint16_t i = 0; i < record_count_; i++) {
    if (records_[i].category == MEM_CAT_SIGNIFICANT) continue;
    if (records_[i].strength < lowest) {
      lowest = records_[i].strength;
      best = i;
    }
  }

  // Swap the weakest record with the last valid record, then point
  // head_ to the now-abandoned slot so the next write fills it.
  if (record_count_ > 0) {
    records_[best] = records_[record_count_ - 1];
    record_count_--;
    head_ = record_count_;

  }
  return lowest;
}

MemorySummary MemoryEngineClass::recall() {
  MemorySummary summary = {};
  uint32_t now = millis();
  uint32_t newest_touch_ms = 0;

  for (uint16_t i = 0; i < record_count_; i++) {
    auto& rec = records_[i];
    if (rec.strength > 0 && rec.domain < DOMAIN_COUNT) {
      uint8_t w = (rec.strength * rec.salience) / 100;
      if (w > summary.domain_strength[rec.domain]) {
        summary.domain_strength[rec.domain] = w;
      }
    }

    if (rec.domain == DOMAIN_TOUCH && rec.formed_at_ms > newest_touch_ms) {
      newest_touch_ms = rec.formed_at_ms;
    }

    if (rec.category == MEM_CAT_EPISODIC) summary.episodic_count++;
    if (rec.category == MEM_CAT_FAMILIARITY) summary.familiarity_count++;
    if (rec.category == MEM_CAT_SIGNIFICANT) summary.significant_present = 1;
  }

  summary.ms_since_last_touch = (newest_touch_ms > 0)
    ? (uint16_t)((now - newest_touch_ms > 0xFFFF) ? 0xFFFF : (now - newest_touch_ms))
    : 0xFFFF;
  summary.total_records = record_count_;
  summary.dropped_touch_events = dropped_touch_events_;

  return summary;
}

void MemoryEngineClass::flush() {
  // No-op in Sprint 1 (in-memory only)
}
