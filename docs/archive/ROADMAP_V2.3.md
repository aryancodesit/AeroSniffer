# AeroSniffer Roadmap (V2.3)

> **Reconstructed from surviving repository artifacts — original file was never committed to git.**  
> The V2.3 `docs/ROADMAP.md` was lost during archival due to case-insensitive filesystem collision.  
> This reconstruction is based on structural clues from `RC1_VALIDATION_PLAN.md`, `V2.4_SPRINT_PLAN.md`, the Character Preservation Initiative described in the V2.5 Companion Architecture, and the Device–Portal–Cloud model documented in the original `ARCHITECTURE.md`. Content and structure are approximate — the original 79-line file cannot be fully recovered.

**Vintage:** V2.3  
**Original path:** `docs/ROADMAP.md`  
**Original length:** 79 lines  

---

## V2.3 Sprints & Milestones

1. **Home Guard Foundation** — Threat detection, portal alerts, trusted device management
2. **Settings Persistence** — NVS-backed configuration save/load across reboots
3. **POST Endpoints** — REST API for Portal configuration and device status
4. **Threat UX Polish** — Visual refinements for Security mode threat display
5. **Flash Device Validation** — Firmware flashing and boot verification pipeline
6. **Real-world Testing** — Hardware deployment and soak testing
7. **Bug Fix Sprint** — Stability fixes from real-world testing findings

---

## Character Preservation Initiative

**Goal:** Ensure AeroSniffer remains engaging as a standalone desktop creature, not just a security peripheral.

### Key Requirements
- Creature feels alive even when no threats or flights are present
- Idle animations, mood shifts, and attention-seeking behavior
- Personality emerges from interaction patterns, not scripted responses

### Design Principles
- Reactive Interface → Companion: creature exists when nothing happens
- Mood as slow behavioral layer (minutes/hours, not seconds)
- Face as primary output channel (gaze, blink, eyelid, particle effects)

---

## Device–Portal–Cloud Model

```
Device = Observe (ESP32-S3 + sensors + TFT)
Portal = Act (mobile app for configuration and alerts)
Cloud  = Analyze (future: fleet management, trend analysis)
```

---

## Legacy

This roadmap was superseded by V2.4 (Home Intelligence) and V2.5 (Creature Brain). The Character Preservation Initiative was realized in V2.5 Sprint 1–5A (Attention, Emotion, Mood, Face, Autonomous Presence). The Device–Portal–Cloud model was refined in V2.5 Companion Architecture.
