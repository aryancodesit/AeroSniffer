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
// Reads MemoryEngine.recall() and applies bounded read-modify-write
// transforms to CreatureState pipeline fields (mood_strength,
// engagement_drive) at pipeline position 6.
//
// Thread model:
//   tick() — called from Core 1 render loop only, after
//            MemoryEngine.tick() and before FaceEngine.render().
//   No EventBus subscriptions, no spinlocks, no cross-core state.
//
// Pipeline transforms (deterministic, O(1), allocation-free):
//   - mood_strength: constrain(baseline + modifier, 0, 100)
//   - engagement_drive: max(current, floor)
//
// _mood_modifier is a persistent accumulator. It accumulates +5
// per tick while touch domain is active (capped ±15) and decays
// linearly at -1/tick toward zero when touch is absent.
// Debug logging is edge-triggered under BEHAVIOR_DEBUG.

class BehaviorEngineClass {
public:
    void begin();
    void tick();

private:
    int8_t  _mood_modifier = 0;
    uint8_t _engagement_floor = 0;

#ifdef BEHAVIOR_DEBUG
    int8_t  _last_logged_mod = -128;
    uint8_t _last_logged_floor = 0xFF;
    uint8_t _last_logged_baseline = 0xFF;
#endif
};

extern BehaviorEngineClass BehaviorEngine;
