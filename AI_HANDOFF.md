# AI Handoff — AeroSniffer V2.5

## Current State

V2.5 Sprint 1 (Companion Intelligence Foundation) and Sprint 2A (FaceEngine Attention Migration) are complete, committed, and hardware-validated. The structured attention model (`target`/`source`/`strength`) is live across all three modes (Pet, Security, Aviation). FaceEngine now consumes the structured fields for gaze direction and magnitude. The V2.4 compatibility shim (`attention_state`) remains active for rollback safety; removal planned for Sprint 2B.

### What's Working
- All three modes boot, render, and respond to touch
- Attention model: structured `target`/`source`/`strength`, fixed-point 3-zone decay, priority preemption, event-to-attention mapping, ring buffer queue with spinlock
- FaceEngine gaze driven by `g_creature.attention.{target,strength}`:
  - TARGET_THREAT: center locked (y=0)
  - TARGET_USER: upward, strength-scaled (-1 to -3)
  - TARGET_FLIGHT: skyward, strength-scaled (-1 to -4)
  - TARGET_NONE + strength≥25: mild curiosity (y=-1)
  - TARGET_NONE + strength<25: random wandering (unchanged)
- `[ATTN] SOFT_FOCUS -> WATCHING (TOUCH_SHORT)` and decay confirmed on hardware
- Mode transitions Pet→Security→Aviation→Pet clean in all directions
- EmotionEngine ticks independently
- Touch input (tap, long press) works across all modes
- WiFi connect/disconnect lifecycle clean on mode entry/exit
- No IWDT, no Guru Meditation, no panics — spinlock fix validated

### What's Disabled (isolation testing)
- `pet_fetch_standalone_weather()` — disabled to avoid `udp_new_ip_type` assertion (B004)
- Mode switch `delay(500)` — removed; 600ms splash kept for visual feedback

### Uncommitted Changes
- None — working tree is clean.

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

### Sprint 2B — Shim Removal
- Remove `attention_state` field from `CreatureState` (`AeroSnifferOS.h:136`)
- Remove shim write from `AeroSniffer.ino:577`
- Remove shim write from `AttentionEngine::publish()` (`AttentionEngine.cpp:166`)
- (Optional) remove `getState()` from `AttentionEngine.h` if no other callers

### Sprint 3 — Mood System
- EmotionLayer (if scoped)
- MoodLayer (if scoped)
- MemoryLayer (if scoped)

## File Map
- `AeroSniffer/AeroSnifferOS.h:97-137` — `AttentionTarget`, `AttentionSource` enums, `CreatureState` with structured `attention` + shim
- `AeroSniffer/AttentionEngine.cpp` — queue, state machine, decay, publish, event mapping (sketch root)
- `AeroSniffer/Companion/AttentionEngine.h` — class declaration, spinlock, structured attention fields
- `AeroSniffer/AeroSnifferOS.cpp:571-602` — FaceEngine gaze driven by `target`/`strength`
- `AeroSniffer/AeroSniffer.ino:577` — shim write (to be removed in Sprint 2B)
- `AeroSniffer/AeroSnifferOS.cpp` — global `AttentionEngine` + `FaceEngine` instances
- `docs/V2.5_SPRINT1_SPEC.md` — Sprint 1 implementation specification
- `docs/V2.5_ATTENTION_MODEL.md` — attention data model

## Git
- Branch: `feature/v2.5-creature-brain`
- Tag: (next release)
- Commits: Sprint 1 (`16db15f`), Dependabot (`6a900b2`), Governance (`69ea694`), Sprint 2A (`c3a6480`)
- Upstream: `origin/feature/v2.5-creature-brain`
