// ================================================================
//  Companion/AttentionEngine.h  —  Visual Attention System
//  AeroSniffer Companion Layer | C++ Class
// ================================================================
#pragma once

#include <Arduino.h>
#include "AeroSnifferOS.h"

// ── Attention Engine ─────────────────────────────────────────────
// Singleton that decides where the creature looks. Accepts EventBus
// events via queueEvent(), drains them in tick(), and transitions
// between SOFT_FOCUS, WATCHING, and THREAT_LOCK.
//
// Thread model:
//   queueEvent() — called from Core 0 (WiFi, security, aviation) and
//                  Core 1 (touch). Writers are guarded by a spinlock.
//   tick()       — called from Core 1 render loop only. Consumer is
//                  lock-free (single reader).
//   pause/resume — called from Core 1 mode switch handler.

class AttentionEngineClass {
public:
  // ── Lifecycle ──
  void begin();
  void tick(uint32_t delta_ms);
  void pause();
  void resume();
  void queueEvent(EventType event, void* data);
  void publish();

  // ── States ──
  enum AttentionState : uint8_t {
    STATE_SOFT_FOCUS  = 0,
    STATE_WATCHING    = 1,
    STATE_THREAT_LOCK = 2
  };
  AttentionState getState() const { return _state; }

private:
  // ── Event Queue ──────────────────────────────────────────────
  static constexpr size_t QUEUE_SIZE = 16;

  struct QueueEntry {
    EventType event;
    void*     data;
  };

  QueueEntry       _queue[QUEUE_SIZE];
  volatile size_t  _head = 0;
  volatile size_t  _tail = 0;

  // ── Spinlock ─────────────────────────────────────────────────
  portMUX_TYPE     _mux = portMUX_INITIALIZER_UNLOCKED;

  // ── Pause State ──────────────────────────────────────────────
  bool             _paused = false;
  bool             _begun  = false;

  // ── State Machine ────────────────────────────────────────────
  AttentionState   _state           = STATE_SOFT_FOCUS;
  uint8_t          _active_priority = 0;   // 0 = none, 1 = highest

  // ── V2.5 Structured Attention ────────────────────────────────
  AttentionTarget   _target           = TARGET_NONE;
  AttentionSource   _source           = SOURCE_INTERNAL;
  uint8_t           _strength         = 0;
  uint32_t          _acquired_ms      = 0;   // millis() when current target was set
  uint8_t           _initial_strength = 0;

  // ── Statistics ───────────────────────────────────────────────
  uint32_t         _dropped_count     = 0;
  uint32_t         _processed_count   = 0;
  uint32_t         _queue_high_water  = 0;

  // ── Priority / Decay Lookup ──────────────────────────────────
  static uint8_t   eventPriority(EventType e);
  static const char* stateName(AttentionState s);
  static const char* eventName(EventType e);

  void resetToIdle();
  static void mapEvent(EventType e, AttentionTarget& target,
                       AttentionSource& source, uint8_t& strength);
  static uint8_t computeStrength(uint8_t initial, uint32_t elapsed_ms);

  // ── Helpers ──────────────────────────────────────────────────
  size_t queueCount() const;
  bool   isQueueFull() const;
  bool   isQueueEmpty() const;
};
