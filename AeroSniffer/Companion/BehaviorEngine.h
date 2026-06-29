// ================================================================
//  Companion/BehaviorEngine.h  —  Memory-Informed Behavioral Layer
//  AeroSniffer Companion Layer | C++ Class
// ================================================================
#pragma once

#include <Arduino.h>
#include "AeroSnifferOS.h"
#include "Memory/MemoryTypes.h"
#include "Memory/MemoryEngine.h"

// ── Behavior Engine ─────────────────────────────────────────────
// Sole canonical producer of engagement_drive (V2.7.2).  Owns the
// full engagement lifecycle: decay, mood multiplier, memory
// modulation, touch/attention reset, and canonical write-back.
//
// Also applies bounded read-modify-write transforms to CreatureState
// mood_strength (transforms MoodEngine baseline) at pipeline pos 6.
//
// Thread model:
//   tick() — called from Core 1 render loop only, after
//            MemoryEngine.tick() and before FaceEngine.render().
//   No EventBus subscriptions, no spinlocks, no cross-core state.
//
// Pipeline transforms (deterministic, O(1), allocation-free):
//   - engagement_drive: full lifecycle (decay → memory mod → reset
//                        → write-back → security floor override)
//   - mood_strength: constrain(baseline + modifier, 0, 100)
//
// _mood_modifier is a persistent accumulator. It accumulates +5
// per tick while touch domain is active (capped ±15) and decays
// linearly at -1/tick toward zero when touch is absent.
// _engagement_level is a private float cache for the engagement
// decay computation.  Its canonical output is g_creature.engagement_drive.
// Debug logging is edge-triggered under BEHAVIOR_DEBUG.

class BehaviorEngineClass {
public:
    void begin();
    void tick();

private:
    int8_t  _mood_modifier = 0;
    uint8_t _engagement_floor = 0;
    float   _engagement_level = 100.0f;
    uint32_t _last_tick_ms = 0;

#ifdef BEHAVIOR_DEBUG
    int8_t  _last_logged_mod = -128;
    uint8_t _last_logged_floor = 0xFF;
    uint8_t _last_logged_baseline = 0xFF;
#endif
};

extern BehaviorEngineClass BehaviorEngine;
