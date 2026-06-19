// ================================================================
//  AttentionEngine.cpp  —  Visual Attention System
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

  _state           = STATE_SOFT_FOCUS;
  _active_priority = 0;
  _decay_deadline  = 0;

  Serial.println("[ATTN] begin -> SOFT_FOCUS");
}

void AttentionEngineClass::tick(uint32_t delta_ms) {
  if (!_begun || _paused) return;

  (void)delta_ms;
  uint32_t now = millis();

  // ── 1. Drain queue ───────────────────────────────────────────
  // Process every queued event. The last event's priority determines
  // the final state. Transition info is captured for serial logging
  // outside the critical section. millis() is read before the lock
  // — the critical section should contain only small deterministic work.
  bool did_transition = false;
  EventType trigger_ev = EVENT_COUNT;
  AttentionState old_state = STATE_SOFT_FOCUS;

  // Future (Sprint 2): cap iterations to max_events_per_tick to bound
  // CPU time during deauth storms. A burst of 16 events in one frame
  // monopolises Core 1 at the expense of rendering.
  portENTER_CRITICAL(&_mux);
  while (_head != _tail) {
    EventType ev = _queue[_tail].event;
    _tail = (_tail + 1) % QUEUE_SIZE;
    _processed_count++;

    uint8_t pri = eventPriority(ev);
    if (pri == 0) continue;

    if (_active_priority == 0 || pri <= _active_priority) {
      AttentionState target = (pri == 1) ? STATE_THREAT_LOCK : STATE_WATCHING;
      if (target != _state) {
        did_transition = true;
        trigger_ev     = ev;
        old_state      = _state;
        _state         = target;
      }
      _active_priority = pri;
      _decay_deadline  = now + decayForPriority(pri);
    }
  }
  portEXIT_CRITICAL(&_mux);

  if (did_transition) {
    Serial.printf("[ATTN] %s -> %s (%s)\n",
      stateName(old_state), stateName(_state), eventName(trigger_ev));
  }

  // ── 2. Check decay ──────────────────────────────────────────
  if (_active_priority > 0 && now >= _decay_deadline) {
    resetState();
  }
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
  resetState();
}

// ── Event Ingestion ──────────────────────────────────────────────

void AttentionEngineClass::queueEvent(EventType event, void* data) {
  if (!_begun) return;

  portENTER_CRITICAL(&_mux);

  // Silently discard events while paused — mode isolation
  if (_paused) {
    portEXIT_CRITICAL(&_mux);
    return;
  }

  // Overflow: discard oldest (advance tail), keep newest
  if (isQueueFull()) {
    _tail = (_tail + 1) % QUEUE_SIZE;
    _dropped_count++;
  }

  _queue[_head] = { event, data };
  _head = (_head + 1) % QUEUE_SIZE;

  // Track queue depth for debugging
  size_t count = (_head + QUEUE_SIZE - _tail) % QUEUE_SIZE;
  if (count > _queue_high_water) {
    _queue_high_water = count;
  }

  portEXIT_CRITICAL(&_mux);
}

// ── State Machine ─────────────────────────────────────────────────

void AttentionEngineClass::resetState() {
  if (_state == STATE_SOFT_FOCUS && _active_priority == 0) return;

  AttentionState old = _state;
  _state           = STATE_SOFT_FOCUS;
  _active_priority = 0;
  _decay_deadline  = 0;

  Serial.printf("[ATTN] %s -> %s (decay)\n", stateName(old), stateName(_state));
}

// ── Priority / Decay Lookup ──────────────────────────────────────

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

uint32_t AttentionEngineClass::decayForPriority(uint8_t pri) {
  switch (pri) {
    case 1:  return 8000;
    case 2:  return 3000;
    case 3:  return 5000;
    case 4:  return 2500;
    default: return 0;
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
