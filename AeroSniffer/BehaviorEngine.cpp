// ================================================================
//  BehaviorEngine.cpp  —  Memory-Informed Behavioral Layer
//  AeroSniffer Companion Layer | C++ Implementation
// ================================================================
#include "Companion/BehaviorEngine.h"
#include <math.h>

BehaviorEngineClass BehaviorEngine;

// ── Engagement Drive Constants (V2.7.2) ────────────────────────
static constexpr float kEngagementDecayPerSec = 0.333f;

static float engagementMoodMultiplier(MoodType m) {
    switch (m) {
        case MOOD_PLAYFUL: return 0.5f;  // stays alert 2x longer
        case MOOD_ANXIOUS: return 0.3f;  // barely decays — hyper-vigilant
        default:           return 1.0f;  // RELAXED — normal decay
    }
}

void BehaviorEngineClass::begin() {
    _mood_modifier = 0;
    _engagement_floor = 0;
    _engagement_level = 100.0f;
    _last_tick_ms = millis();
    g_creature.engagement_drive = 100;
}

void BehaviorEngineClass::tick() {
    uint32_t now = millis();
    uint32_t dt_ms = now - _last_tick_ms;
    _last_tick_ms = now;

    MemorySummary mem = MemoryEngine.recall();

    // ── Capture pre-transform baseline for debug ─────
#ifdef BEHAVIOR_DEBUG
    uint8_t pre_mood = g_creature.mood_strength;
    uint8_t pre_engage = g_creature.engagement_drive;
#endif

    // ════════════════════════════════════════════════════════════
    //  ENGAGEMENT DRIVE LIFECYCLE (V2.7.2)
    //  Sole canonical producer of g_creature.engagement_drive.
    // ════════════════════════════════════════════════════════════

    // Decay toward 0 over time; reset on any interaction.
    float decay = kEngagementDecayPerSec * engagementMoodMultiplier(g_creature.mood) * (dt_ms / 1000.0f);
    float floor_val = (g_creature.mood == MOOD_ANXIOUS) ? 20.0f : 0.0f;

    // ── Memory Layer modulation ────────────────────────────────
    if (mem.ms_since_last_touch < 1800000) {
        decay *= 0.80f;
    }
    if (mem.ms_since_last_touch >= 1800000 && mem.domain_strength[DOMAIN_TOUCH] < 10) {
        floor_val = max(floor_val, 5.0f);
    }

    _engagement_level = max(floor_val, _engagement_level - decay);

    // Reset on touch or any attention target (orienting response)
    if (g_creature.attention.source == SOURCE_TOUCH || g_creature.attention.target != TARGET_NONE) {
        _engagement_level = 100.0f;
    }

    // Canonical write-back
    g_creature.engagement_drive = (uint8_t)(_engagement_level + 0.5f);

    // ════════════════════════════════════════════════════════════
    //  RULES
    // ════════════════════════════════════════════════════════════

    int8_t delta = 0;
    uint8_t floor = 0;

    // Rule 1 — Security threat: floor engagement at 60 when
    //           domain strength exceeds mood baseline dominance.
    if (mem.domain_strength[DOMAIN_SECURITY] > 30 &&
        mem.domain_strength[DOMAIN_SECURITY] > g_creature.mood_strength) {
        floor = 60;
    }

    // Rule 2 — Recent touch history: amplify mood expression
    //           by accumulating +5 per tick (capped at ±15).
    if (mem.domain_strength[DOMAIN_TOUCH] > 25) {
        delta += 5;
    }

    // Rule 3 — Aviation: deferred (no V1 action).
    //           Future: recent flight event within 60s triggers
    //           attention_bias toward TARGET_FLIGHT.

    // ── Decay persistent modifier toward zero ─────────
    if (_mood_modifier > 0) {
        _mood_modifier--;
    } else if (_mood_modifier < 0) {
        _mood_modifier++;
    }

    // ── Accumulate delta from active rules ─────────────
    _mood_modifier = (int8_t)constrain(
        (int16_t)_mood_modifier + delta, -15, 15
    );

    // ── Apply pipeline transforms (read-modify-write) ─
    g_creature.mood_strength = constrain(
        g_creature.mood_strength + _mood_modifier, 0, 100
    );
    g_creature.engagement_drive = max(
        g_creature.engagement_drive, floor
    );

    _engagement_floor = floor;

    // ── Edge-triggered debug log ─────────────────────
#ifdef BEHAVIOR_DEBUG
    if (_mood_modifier != _last_logged_mod ||
        _engagement_floor != _last_logged_floor ||
        pre_mood != _last_logged_baseline) {
        Serial.printf("[BEH] baseline=%d mod=%d prev_engage=%d floor=%d\n",
            pre_mood, _mood_modifier, pre_engage, _engagement_floor);
        _last_logged_mod = _mood_modifier;
        _last_logged_floor = _engagement_floor;
        _last_logged_baseline = pre_mood;
    }
#endif
}
