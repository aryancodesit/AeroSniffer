// ================================================================
//  AttentionEngine.cpp  —  Visual Attention System (V2.5)
//  AeroSniffer Companion Layer | C++ Implementation
// ================================================================
#include "Companion/AttentionEngine.h"

// ── Lifecycle ────────────────────────────────────────────────────

void AttentionEngineClass::begin() {
  _head            = 0;
  _tail            = 0;
  _dropped_count   = 0;
  _processed_count = 0;
  _queue_high_water = 0;
  _paused          = false;
  _begun           = true;

  resetToIdle();
  Serial.println("[ATTN] begin -> SOFT_FOCUS");
}

void AttentionEngineClass::tick(uint32_t delta_ms) {
  if (!_begun || _paused) return;

  (void)delta_ms;
  uint32_t now = millis();

  // ── 1. Drain queue ───────────────────────────────────────────
  // Lock-free single consumer on Core 1. Producers (queueEvent)
  // write _head under spinlock; consumer reads _head, reads entry,
  // advances _tail. volatile prevents compiler-cache stalls.
  // Worst case: missed event (stale _head read) — next tick picks it up.
  bool did_event_transition = false;
  EventType trigger_ev = EVENT_COUNT;
  AttentionState old_state = _state;

  static constexpr size_t MAX_EVENTS_PER_TICK = 8;
  size_t events_this_tick = 0;
  while (_head != _tail && events_this_tick < MAX_EVENTS_PER_TICK) {
    events_this_tick++;
    EventType ev = _queue[_tail].event;
    _tail = (_tail + 1) % QUEUE_SIZE;
    _processed_count++;

    uint8_t pri = eventPriority(ev);
    if (pri == 0) continue;

    // Accept: preempt (pri <= current) or first event
    if (_active_priority == 0 || pri <= _active_priority) {
      mapEvent(ev, _target, _source, _initial_strength);
      _acquired_ms = now;
      _active_priority = pri;

      AttentionState target_state = (_target == TARGET_THREAT)  ? STATE_THREAT_LOCK :
                                    (_target != TARGET_NONE)    ? STATE_WATCHING :
                                                                  STATE_SOFT_FOCUS;
      if (target_state != _state) {
        did_event_transition = true;
        trigger_ev = ev;
        old_state  = _state;
        _state     = target_state;
      }
    }
  }

  // ── 2. Compute decay ────────────────────────────────────────
  if (_active_priority > 0) {
    uint32_t elapsed = now - _acquired_ms;
    if (elapsed > 30000) {
      resetToIdle();
    } else {
      _strength = computeStrength(_initial_strength, elapsed);
      if (_strength == 0) {
        resetToIdle();
      }
    }
  }

  // ── 3. Log event-driven transition ──────────────────────────
  // (decay-driven transitions are logged inside resetToIdle())
  if (did_event_transition) {
    Serial.printf("[ATTN] %s -> %s (%s)\n",
      stateName(old_state), stateName(_state), eventName(trigger_ev));
  }

  // ── 4. Publish to CreatureState ─────────────────────────────
  publish();
}

// ── Mode Transition ──────────────────────────────────────────────

void AttentionEngineClass::pause() {
  if (!_begun) return;
  if (!_paused) {
    Serial.printf("[ATTN] pause (was %s)\n", stateName(_state));
  }
  // No critical section needed — _paused flag is the exclusive guard.
  // queueEvent() discards events while paused, tick() returns early.
  // Pre-pause events remain in the queue and drain naturally on
  // the next tick() after resume().  Removing the spinlock eliminates
  // the Interrupt WDT that occurred when Core 0 held _mux during
  // queueEvent() while Core 1 tried to enter pause().
  _paused = true;
}

void AttentionEngineClass::resume() {
  if (!_begun) return;
  if (_paused) {
    Serial.println("[ATTN] resume -> SOFT_FOCUS");
  }
  _paused = false;
  resetToIdle();
}

// ── Event Ingestion ──────────────────────────────────────────────

void AttentionEngineClass::queueEvent(EventType event, void* data) {
  if (!_begun) return;

  portENTER_CRITICAL(&_mux);

  if (_paused) {
    portEXIT_CRITICAL(&_mux);
    return;
  }

  if (isQueueFull()) {
    _tail = (_tail + 1) % QUEUE_SIZE;
    _dropped_count++;
  }

  _queue[_head] = { event, data };
  _head = (_head + 1) % QUEUE_SIZE;

  size_t count = (_head + QUEUE_SIZE - _tail) % QUEUE_SIZE;
  if (count > _queue_high_water) {
    _queue_high_water = count;
  }

  portEXIT_CRITICAL(&_mux);
}

// ── V2.5 Structured Attention ─────────────────────────────────────

void AttentionEngineClass::resetToIdle() {
  if (_target == TARGET_NONE && _active_priority == 0) return;

  AttentionState old = _state;
  _target           = TARGET_NONE;
  _source           = SOURCE_INTERNAL;
  _strength         = 0;
  _state            = STATE_SOFT_FOCUS;
  _active_priority  = 0;
  _acquired_ms      = 0;
  _initial_strength = 0;

  Serial.printf("[ATTN] %s -> %s (decay)\n", stateName(old), stateName(_state));
}

void AttentionEngineClass::publish() {
  g_creature.attention.target   = _target;
  g_creature.attention.source   = _source;
  g_creature.attention.strength = _strength;

  // V2.4 compatibility shim — remove in Phase B
  g_creature.attention_state    = (uint8_t)_state;
}

// ── Event → Attention Mapping ────────────────────────────────────

void AttentionEngineClass::mapEvent(EventType e,
    AttentionTarget& target, AttentionSource& source, uint8_t& strength) {
  switch (e) {
    case EVENT_ATTACK_DEAUTH:
    case EVENT_ATTACK_EVILTWIN:
      target = TARGET_THREAT; source = SOURCE_SECURITY; strength = 100; break;
    case EVENT_TOUCH_SHORT:
    case EVENT_TOUCH_LONG:
      target = TARGET_USER; source = SOURCE_TOUCH; strength = 55; break;
    case EVENT_FLIGHT_DETECTED:
      target = TARGET_FLIGHT; source = SOURCE_AVIATION; strength = 45; break;
    case EVENT_FLIGHT_RARE:
      target = TARGET_FLIGHT; source = SOURCE_AVIATION; strength = 55; break;
    case EVENT_WIFI_CONNECTING:
    case EVENT_WIFI_DISCONNECTED:
      target = TARGET_NONE; source = SOURCE_INTERNAL; strength = 30; break;
    case EVENT_WIFI_CONNECTED:
      target = TARGET_NONE; source = SOURCE_INTERNAL; strength = 25; break;
    case EVENT_MODE_SWITCHED:
      target = TARGET_NONE; source = SOURCE_INTERNAL; strength = 20; break;
    default:
      target = TARGET_NONE; source = SOURCE_INTERNAL; strength = 0; break;
  }
}

// ── Strength Decay (Fixed-Point, 3-Zone) ─────────────────────────

uint8_t AttentionEngineClass::computeStrength(uint8_t initial, uint32_t elapsed_ms) {
  int s = (int)initial;
  uint32_t t = elapsed_ms;

  // Zone 1: above 50, decays at 25/s  (40ms per unit)
  if (s > 50) {
    uint32_t zone_ms = (s - 50) * 40;
    if (t < zone_ms) {
      s -= (int)((25 * t + 500) / 1000);
      return (uint8_t)(s < 0 ? 0 : (s > 100 ? 100 : s));
    }
    t -= zone_ms;
    s = 50;
  }

  // Zone 2: 20–50, decays at 10/s  (100ms per unit)
  if (s > 20) {
    uint32_t zone_ms = (s - 20) * 100;
    if (t < zone_ms) {
      s -= (int)((10 * t + 500) / 1000);
      return (uint8_t)(s < 0 ? 0 : (s > 100 ? 100 : s));
    }
    t -= zone_ms;
    s = 20;
  }

  // Zone 3: below 20, decays at 5/s  (200ms per unit)
  {
    uint32_t zone_ms = s * 200;
    if (t < zone_ms) {
      s -= (int)((5 * t + 500) / 1000);
    } else {
      s = 0;
    }
  }

  return (uint8_t)(s < 0 ? 0 : (s > 100 ? 100 : s));
}

// ── Priority Lookup ──────────────────────────────────────────────

uint8_t AttentionEngineClass::eventPriority(EventType e) {
  switch (e) {
    case EVENT_ATTACK_DEAUTH:
    case EVENT_ATTACK_EVILTWIN:
      return 1;
    case EVENT_TOUCH_SHORT:
    case EVENT_TOUCH_LONG:
      return 2;
    case EVENT_FLIGHT_DETECTED:
    case EVENT_FLIGHT_RARE:
      return 3;
    case EVENT_WIFI_CONNECTING:
    case EVENT_WIFI_CONNECTED:
    case EVENT_WIFI_DISCONNECTED:
    case EVENT_MODE_SWITCHED:
      return 4;
    default:
      return 0;
  }
}

const char* AttentionEngineClass::stateName(AttentionState s) {
  switch (s) {
    case STATE_SOFT_FOCUS:  return "SOFT_FOCUS";
    case STATE_WATCHING:    return "WATCHING";
    case STATE_THREAT_LOCK: return "THREAT_LOCK";
    default:                return "?";
  }
}

const char* AttentionEngineClass::eventName(EventType e) {
  switch (e) {
    case EVENT_ATTACK_DEAUTH:    return "ATTACK_DEAUTH";
    case EVENT_ATTACK_EVILTWIN:  return "ATTACK_EVILTWIN";
    case EVENT_TOUCH_SHORT:      return "TOUCH_SHORT";
    case EVENT_TOUCH_LONG:       return "TOUCH_LONG";
    case EVENT_FLIGHT_DETECTED:  return "FLIGHT_DETECTED";
    case EVENT_FLIGHT_RARE:      return "FLIGHT_RARE";
    case EVENT_WIFI_CONNECTING:  return "WIFI_CONNECTING";
    case EVENT_WIFI_CONNECTED:   return "WIFI_CONNECTED";
    case EVENT_WIFI_DISCONNECTED: return "WIFI_DISCONNECTED";
    case EVENT_MODE_SWITCHED:    return "MODE_SWITCHED";
    default:                     return "UNKNOWN";
  }
}

// ── Private Helpers ──────────────────────────────────────────────

size_t AttentionEngineClass::queueCount() const {
  return (_head + QUEUE_SIZE - _tail) % QUEUE_SIZE;
}

bool AttentionEngineClass::isQueueFull() const {
  return ((_head + 1) % QUEUE_SIZE) == _tail;
}

bool AttentionEngineClass::isQueueEmpty() const {
  return _head == _tail;
}
