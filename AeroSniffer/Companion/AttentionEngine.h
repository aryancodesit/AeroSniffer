// ================================================================
//  Companion/AttentionEngine.h  —  Visual Attention System
//  AeroSniffer Companion Layer | C++ Class
// ================================================================
#pragma once

#include <Arduino.h>
#include "AeroSnifferOS.h"

// ── Attention Engine ─────────────────────────────────────────────
// Singleton that decides where the creature looks. Accepts EventBus
// events via queueEvent(), drains them in tick(), and will
// (in Phase 2) produce gaze_x/gaze_y/blink_request for FaceEngine.
//
// Thread model:
//   queueEvent() — called from Core 0 (WiFi, security, aviation) and
//                  Core 1 (touch). Writers are guarded by a spinlock.
//   tick()       — called from Core 1 render loop only. Consumer is
//                  lock-free (single reader).
//   pause/resume — called from Core 1 mode switch handler.

class AttentionEngineClass {
public:
  void begin();
  void tick(uint32_t delta_ms);
  void pause();
  void resume();
  void queueEvent(EventType event, void* data);

  // ── Phase 2 stubs ────────────────────────────────────────────
  // gaze_x, gaze_y, blink_request output
  // active_state, priority, duration context for EmotionEngine
  // habituation counters

private:
  // ── Event Queue ──────────────────────────────────────────────
  // Fixed-size ring buffer. Multi-producer (Core 0 + 1), single
  // consumer (Core 1 tick). Overflow: discard oldest, keep newest.
  static constexpr size_t QUEUE_SIZE = 16;

  struct QueueEntry {
    EventType event;
    void*     data;     // Phase 2: extract/copy at publish time
  };

  QueueEntry       _queue[QUEUE_SIZE];
  size_t           _head = 0;
  size_t           _tail = 0;

  // ── Spinlock (guards queueEvent writes only) ─────────────────
  portMUX_TYPE     _mux = {};   // zero-init = unlocked

  // ── Pause State ──────────────────────────────────────────────
  bool             _paused = false;
  bool             _begun  = false;

  // ── Statistics ───────────────────────────────────────────────
  uint32_t         _dropped_count     = 0;
  uint32_t         _processed_count   = 0;
  uint32_t         _queue_high_water  = 0;

  // ── Helpers ──────────────────────────────────────────────────
  size_t queueCount() const;
  bool   isQueueFull() const;
  bool   isQueueEmpty() const;
};
