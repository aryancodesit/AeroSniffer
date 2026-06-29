# AeroSniffer Architecture

## Repository Structure

```
AeroSniffer/
├── AeroSniffer.ino         — Entry point, FreeRTOS tasks (Core 0 / Core 1)
├── AeroSnifferOS.h         — CreatureState, enums, globals, externs
├── AeroSnifferOS.cpp       — Global state, engine lifecycle
├── AttentionEngine.cpp     — Touch→attention state machine (sketch root)
├── MoodEngine.cpp          — Mood arbitration, fixed-point decay
├── MemoryEngine.cpp        — Ring buffer, domain formation, decay, recall
├── PersistenceService.cpp  — NVS blink store, schema migration
├── BehaviorEngine.cpp      — [Planned V2.7] Memory consumer, CreatureState influencer
├── Config.h                — Timing constants (TAP_MAX_MS, etc.)
├── Companion/
│   ├── AttentionEngine.h   — Spinlock, structured attention fields
│   ├── EmotionEngine.h     — Emotion state machine
│   ├── MoodEngine.h        — Touch history ring buffer, arbitration logic
│   ├── BehaviorEngine.h    — [Planned V2.7] Behavior Layer declaration
│   └── FaceEngine.h        — [Defined in AeroSnifferOS.h]
├── Memory/
│   ├── MemoryEngine.h      — Class declaration, domain methods
│   └── MemoryTypes.h       — Domain enums, MemoryRecord, MemorySummary
├── Persistence/
│   └── PersistenceService.h — CreatureProfile struct
├── Security/               — Portal mode (AP, WebServer, threat engine)
├── Aviation/               — OpenSky client, aircraft rendering
├── Pet/                    — Companion mode tasks
├── Modes/                  — Mode manager, mode lifecycle
└── Services/               — WiFi, BLE, WebServer, EventBus
assets/
├── faces/                  — BMP face assets (design-time)
docs/                       — Specifications, results, architecture
├── archive/                — Obsolete planning documents (historical record)
tools/                      — CI pipeline, evidence framework, test signatures
companion-app/              — Companion mobile application
pc-agent/                   — PC agent (serial daemon)
.github/workflows/          — CI pipeline (firmware-build.yml)
```

## Subsystem Architecture

### Current (V2.6)

```
 Touch/Sensors
      ↓
 AttentionEngine  ──→  EmotionEngine  ──→  MoodEngine  ──→  FaceEngine
                                                               ↑
                                                          PersistenceService (observer)
                                                          MemoryEngine (observer)
```

| Engine | Responsibility | Reads | Writes |
|--------|---------------|-------|--------|
| AttentionEngine | Touch event → attention target/strength | Touch, EventBus | `CreatureState.attention` |
| EmotionEngine | Attention → emotion classification | `CreatureState.attention` | `CreatureState.emotion` |
| MoodEngine | Attention + emotion + touch history → mood arbitration | Touch history, `CreatureState.(attention,emotion)` | `CreatureState.(mood,mood_strength)` |
| MemoryEngine | Memory formation, decay, dedup, LittleFS persistence | Touch/security/aviation events | Internal ring buffer, LittleFS |
| PersistenceService | Cross-session profile save/load | `CreatureState.(attention,mood)` | NVS Preferences |
| FaceEngine | State → TFT rendering | `CreatureState` (all fields) | TFT display buffer |

All data flow is unidirectional and acyclic. No engine reads what another engine wrote in the same tick.

### Stage Ownership Rule

> Every pipeline stage has exclusive ownership of the fields it transforms while executing. After the stage completes, ownership passes to the next stage.

This is the core architectural principle of AeroSniffer. Behavior engines transform state. Presentation engines interpret state. Rendering code must never define creature behavior.

### Canonical State Ownership

| Field | Baseline Producer | Canonical Producer | Consumer(s) |
|---|---|---|---|
| `emotion` | EmotionEngine | EmotionEngine | MoodEngine, FaceEngine |
| `mood` | MoodEngine | MoodEngine | BehaviorEngine, FaceEngine |
| `mood_strength` | MoodEngine | BehaviorEngine | FaceEngine |
| `engagement_drive` | — | BehaviorEngine | FaceEngine |
| `attention.*` | AttentionEngine | AttentionEngine | BehaviorEngine, FaceEngine |
| `activity` | EmotionEngine | EmotionEngine | FaceEngine |
| Infrastructure fields | EmotionEngine / process_serial_commands | EmotionEngine / process_serial_commands | PersistenceService |

Every `CreatureState` field has exactly one canonical producer. Baselines are intermediate pipeline values that are transformed by downstream stages before the final canonical write. This table is the authoritative reference for ownership during code review. Any field written by a non-owner requires an explicit ADR.

### Current (V2.7.2)

```
 Touch/Sensors
      ↓
 AttentionEngine  ──→  EmotionEngine  ──→  MoodEngine  ──→  FaceEngine
                                                               ↑
                                                          PersistenceService (observer)
                                                          MemoryEngine → BehaviorEngine
                                                                              ↓
                                                                        CreatureState
```

BehaviorEngine reads `MemoryEngine.recall()` and serves as the sole canonical producer of `engagement_drive`. It transforms MoodEngine's baseline `mood_strength` into the final canonical value. FaceEngine is a pure presentation engine — it reads CreatureState and never writes behavioral fields.

## Three-Mode Lifecycle

| Mode | Purpose | Entry | Exit |
|------|---------|-------|------|
| Companion | Pet interface, idle interaction | Default on boot | User-initiated |
| Security | Threat detection, portal | Mode switch | Mode switch |
| Aviation | ADS-B/Mode-S flight tracking | Mode switch | Mode switch |

Each mode has its own task with WiFi lifecycle (connect/disconnect), rendering loop, and mode-specific sensors. Mode transitions preserve CreatureState for cross-mode carryover.

## EventBus Role

EventBus is used for internal event distribution (touch events, mode transitions). It is NOT used by:
- AttentionEngine (uses direct spinlock-guarded ring buffer)
- MemoryEngine (observes via direct hooks, not EventBus subscription)
- PersistenceService (uses rising-edge counter hooks)

## Cross-Core Task Model

| Core | Tasks | Responsibility |
|------|-------|---------------|
| Core 0 (Protocol) | WiFi, BLE, WebServer, EventBus, sensor I/O | Background services, network, interrupt handling |
| Core 1 (Application) | Attention → Emotion → Mood → Face → Memory tick loop | Frame rendering, state machine updates |

FaceEngine runs on Core 1 at ~30 FPS. MemoryEngine.tick() runs after MoodEngine.tick() in the same Core 1 loop.

## V2.7 Architecture Decisions

| Decision | Rationale |
|----------|-----------|
| Behavior Layer as separate engine | Preserves single responsibility — FaceEngine renders, BehaviorEngine interprets memory |
| Memory → Behavior → CreatureState → Face | Unidirectional data flow, no circular dependencies |
| Deterministic rules (V1) | Testable, predictable, no ML dependencies |
| FaceEngine unchanged | No modification to certified subsystem — additive architecture only |
| Sprint 3 HW certification before/during Sprint 1 | Ensures memory domain data quality before Behavior Layer consumes it |
