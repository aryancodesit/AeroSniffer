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
  double_tap_last_ms_ = 0;
  burst_window_start_ = 0;
  burst_count_ = 0;
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

void MemoryEngineClass::observe() {
  uint32_t now = millis();

  if (record_count_ > MEMORY_MAX_RECORDS) {
    record_count_ = MEMORY_MAX_RECORDS;
  }

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

      const char* name =
        (subtype == TOUCH_TAP) ? "TAP" :
        (subtype == TOUCH_DOUBLE) ? "DOUBLE" :
        (subtype == TOUCH_HOLD) ? "HOLD" :
        (subtype == TOUCH_LONG_HOLD) ? "LONG_HOLD" : "?";
      Serial.printf("[MEM] formed TOUCH %s salience=%d strength=100 duration=%u\n",
        name, sal, duration_ms);
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

        Serial.printf("[MEM] formed TOUCH BURST salience=%d strength=100\n", burst_sal);
      }
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

bool MemoryEngineClass::try_dedup(uint8_t domain, uint8_t subtype, uint8_t salience) {
  uint32_t now = millis();
  for (uint16_t i = 0; i < record_count_; i++) {
    auto& rec = records_[i];
    if (rec.domain == domain && rec.subtype == subtype
        && rec.category == MEM_CAT_EPISODIC
        && (now - rec.formed_at_ms) <= 60000
        && rec.strength > 50) {

      // Boost existing record
      uint8_t new_sal = rec.salience + 10;
      if (new_sal > 100) new_sal = 100;
      rec.salience = new_sal;
      rec.formed_at_ms = now;
      if (rec.strength < 100) rec.strength = 100;

      const char* stype_name =
        (subtype == TOUCH_TAP) ? "TAP" :
        (subtype == TOUCH_DOUBLE) ? "DOUBLE" :
        (subtype == TOUCH_HOLD) ? "HOLD" :
        (subtype == TOUCH_LONG_HOLD) ? "LONG_HOLD" :
        (subtype == TOUCH_BURST) ? "BURST" : "?";
      Serial.printf("[MEM] dedup %s boosted salience=%d\n", stype_name, rec.salience);
      return true;
    }
  }
  return false;
}

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
    Serial.printf("[MEM] evicted record %d (strength=%d)\n", best, lowest);
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
