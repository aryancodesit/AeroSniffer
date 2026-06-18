// ================================================================
//  Companion/AttentionEngine.cpp  —  Visual Attention System
//  AeroSniffer Companion Layer | C++ Implementation
// ================================================================
#include "AttentionEngine.h"

// ── Lifecycle ────────────────────────────────────────────────────

void AttentionEngineClass::begin() {
  _head            = 0;
  _tail            = 0;
  _dropped_count   = 0;
  _processed_count = 0;
  _queue_high_water = 0;
  _paused          = false;
  _begun           = true;
}

void AttentionEngineClass::tick(uint32_t delta_ms) {
  if (!_begun || _paused) return;

  (void)delta_ms;   // unused in Phase 1 — drives decay in Phase 2

  // Drain the entire queue under one critical section.
  // Phase 2 will extract and process each entry here.
  portENTER_CRITICAL(&_mux);
  while (_head != _tail) {
    _tail = (_tail + 1) % QUEUE_SIZE;
    _processed_count++;
  }
  portEXIT_CRITICAL(&_mux);
}

// ── Mode Transition ──────────────────────────────────────────────

void AttentionEngineClass::pause() {
  if (!_begun) return;
  _paused = true;

  // Flush the event queue — events arriving during pause are stale.
  portENTER_CRITICAL(&_mux);
  _head = 0;
  _tail = 0;
  portEXIT_CRITICAL(&_mux);
}

void AttentionEngineClass::resume() {
  if (!_begun) return;
  _paused = false;
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

// ── Private Helpers ──────────────────────────────────────────────

size_t AttentionEngineClass::queueCount() const {
  // Only called from consumer (Core 1 tick) or with spinlock held.
  return (_head + QUEUE_SIZE - _tail) % QUEUE_SIZE;
}

bool AttentionEngineClass::isQueueFull() const {
  return ((_head + 1) % QUEUE_SIZE) == _tail;
}

bool AttentionEngineClass::isQueueEmpty() const {
  return _head == _tail;
}
