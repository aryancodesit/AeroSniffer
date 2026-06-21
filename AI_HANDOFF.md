# AI Handoff — AeroSniffer V2.5

## Current State

V2.5 Sprint 1 (Companion Intelligence Foundation), Sprint 2A (FaceEngine Attention Migration), Sprint 2B (Shim Removal), and Sprint 3A (Mood Infrastructure) are complete, committed, and hardware-validated. The structured attention model (`target`/`source`/`strength`) is live across all three modes. MoodEngine observes CreatureState and derives mood via fixed-priority arbitration (3 of 7 spec'd moods: RELAXED, PLAYFUL, ANXIOUS). FaceEngine consumes structured attention fields for gaze; mood consumption is Sprint 3B.

### What's Working
- All three modes boot, render, and respond to touch
- Attention model: structured `target`/`source`/`strength`, fixed-point 3-zone decay, priority preemption, event-to-attention mapping, ring buffer queue with spinlock
- FaceEngine gaze driven by `g_creature.attention.{target,strength}`:
  - TARGET_THREAT: center locked (y=0)
  - TARGET_USER: upward, strength-scaled (-1 to -3)
  - TARGET_FLIGHT: skyward, strength-scaled (-1 to -4)
  - TARGET_NONE + strength≥25: mild curiosity (y=-1)
  - TARGET_NONE + strength<25: random wandering (unchanged)
- MoodEngine: RELAXED (default), PLAYFUL (≥2 touches/5min + positive emotion 60s recency + 30s hold-off), ANXIOUS (TARGET_THREAT + EMOTION_ALERT, immediate bypass)
- Mood decay: fixed-point, 36s/unit for PLAYFUL (30 min total), 24s/unit for ANXIOUS (20 min), advances `_last_mood_change` by consumed interval steps (not cumulative re-subtraction)
- Touch history ring buffer (10 slots, sliding 5-min window, SOURCE_TOUCH rising edge detection)
- `[ATTN] SOFT_FOCUS -> WATCHING (TOUCH_SHORT)` and decay confirmed on hardware
- `[MOOD] RELAXED -> PLAYFUL -> RELAXED` cycle validated
- Mode transitions Pet→Security→Aviation→Pet clean in all directions
- EmotionEngine ticks independently
- Touch input (tap, long press) works across all modes
- WiFi connect/disconnect lifecycle clean on mode entry/exit
- No IWDT, no Guru Meditation, no panics — spinlock fix validated

### What's Disabled (isolation testing)
- `pet_fetch_standalone_weather()` — disabled to avoid `udp_new_ip_type` assertion (B004)
- Mode switch `delay(500)` — removed; 600ms splash kept for visual feedback

### Uncommitted Changes
- Working tree has Sprint 3A closure edits (PROJECT_STATUS.md, AI_HANDOFF.md, CHANGELOG.md, soak logging removed).

## Known Issues to Resolve (in priority order)

### P1 — Fix Aviation HTTPS fetch (B007)
- [ ] Test HTTP (non-TLS) to isolate TLS vs. DNS vs. routing
- [ ] Verify certificate bundle is present
- [ ] Consider `WiFiClientSecure::setInsecure()` for testing
- [ ] Fix root cause

### P2 — Re-enable weather fetch with lwIP fix (B004)
- [ ] Uncomment `pet_fetch_standalone_weather()` in `Mode1_Pet.h`
- [ ] Apply fix: resolve hostname via `WiFi.hostByName()` before HTTP fetch
- [ ] Or wrap HTTP in `tcpip_adapter_lock()` / use IP literal
- [ ] Verify no `udp_new_ip_type` assertion through multiple mode cycles

### P3 — Investigate `netstack` error (B006)
- [ ] Capture full log of Security→Aviation transition
- [ ] Decode error code 12308
- [ ] Evaluate if netif teardown/setup order needs fixing

### P4 — Touch degradation after WiFi (B008)
- [ ] Investigate `g_block_touch` flag blocking touch after WiFi operations

### P5 — Suppress same-state decay logs (cosmetic)
- [ ] Guard `resetToIdle()` log to only print when `old != _state`
- [ ] Prevents `[ATTN] SOFT_FOCUS -> SOFT_FOCUS (decay)` noise during internal events

## Next Phase

### Sprint 3B — Mood Consumption
- FaceEngine modifiers: blink frequency, idle bounce, animation intensity, expression amplification
- Mood→Emotion feedback explicitly excluded (one-way data flow)
- No new mood generation, no persistence, no portal telemetry

### Future Sprints
- Sprint 4 — Memory Layer (not yet started)

## File Map
- `AeroSniffer/AeroSnifferOS.h:74-79` — `MoodType` enum (RELAXED=0, PLAYFUL=1, ANXIOUS=2, MOOD_COUNT=3)
- `AeroSniffer/AeroSnifferOS.h:113` — `uint8_t mood_strength` in `CreatureState`
- `AeroSniffer/AeroSnifferOS.h:97-133` — `AttentionTarget`, `AttentionSource` enums, `CreatureState` with structured `attention`
- `AeroSniffer/AeroSnifferOS.cpp:571-602` — FaceEngine gaze driven by `target`/`strength`
- `AeroSniffer/AeroSnifferOS.cpp:13-29` — `g_creature` aggregate initializer with `mood_strength=0`
- `AeroSniffer/AttentionEngine.cpp` — queue, state machine, decay, publish, event mapping (sketch root)
- `AeroSniffer/Companion/AttentionEngine.h` — class declaration, spinlock, structured attention fields
- `AeroSniffer/Companion/MoodEngine.h` — class declaration, touch history ring buffer, arbitration
- `AeroSniffer/MoodEngine.cpp` — implementation: fixed-priority arbitration, fixed-point decay, touch tracking
- `AeroSniffer/AeroSniffer.ino:577-583` — tick order: Attention→Emotion→Mood→Face
- `AeroSniffer/AeroSniffer.ino:340` — GET_PET_STATUS: `MoodEngine.moodName(g_creature.mood)`
- `AeroSniffer/Security/Portal.h:1240-1258` — EmotionEngine emotion-only calls (setMood bridge removed)
- `docs/V2.5_SPRINT3_MOOD_SPEC.md` — full Mood System spec (12 sections)
- `docs/V2.5_ATTENTION_RETROSPECTIVE.md` — Sprint 1–2B retrospective
- `docs/V2.5_SPRINT1_SPEC.md` — Sprint 1 implementation specification
- `docs/V2.5_ATTENTION_MODEL.md` — attention data model

## Git
- Branch: `feature/v2.5-creature-brain`
- Tags: `v2.5-attention-complete` (Sprint 2B), `v2.5-mood-foundation` (Sprint 3A)
- Commits: Sprint 1 (`16db15f`), Dependabot (`6a900b2`), Governance (`69ea694`), Sprint 2A (`c3a6480`), Sprint 2B (`05762ec`), Sprint 3A (`(HEAD)`)
- Upstream: `origin/feature/v2.5-creature-brain`
