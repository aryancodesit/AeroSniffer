# Contributing to AeroSniffer

Thanks for your interest. AeroSniffer is a single-maintainer embedded project,
so contributions should respect the existing architecture and review process.

## How to Contribute

### Report a Bug

Open a [Bug Report](.github/ISSUE_TEMPLATE/bug_report.md) and include:

- Hardware: DeskBuddy 2.0 or DevKitC?
- Mode: Companion / Security / Aviation?
- Serial log from boot to the failure
- What you expected vs. what happened

Do not post credentials, tokens, or network topology in issues.

### Request a Feature

Open a [Feature Request](.github/ISSUE_TEMPLATE/feature_request.md). Explain:

- What problem it solves
- How it fits the existing architecture (observe → attention → emotion → mood → face)
- Whether it requires new hardware

### Submit a Pull Request

1. **Start from an issue.** Open a discussion first for non-trivial changes.
2. **Keep scope narrow.** One feature or fix per PR.
3. **Match the architecture.** Observer subsystems for new data; no EventBus
   subscriptions in render code; no spinlock surprises.
4. **Test on hardware.** Flashing to an ESP32-S3 (DeskBuddy 2.0) is required.
5. **Run the checklist.** See the PR template.

## Development Setup

### Hardware

- DeskBuddy 2.0 (XIAO ESP32S3 + ST7789 240x240)
- USB-C cable for power + serial

### Software

- Arduino IDE (or PlatformIO if you prefer)
- ESP32 Arduino core 3.x (ESP-IDF v5.x)
- Libraries: TFT_eSPI, ArduinoJson, ArduinoFFT

### Flashing

```bash
# Arduino IDE
# 1. Select board: "XIAO_ESP32S3"
# 2. Select port (COM* on Windows)
# 3. Upload
```

First flash requires configuring TFT_eSPI `User_Setup.h` for DeskBuddy 2.0
pins. See the repository's `TFT_eSPI_UserSetup.h` for the correct config.

## Project Structure

```
AeroSniffer/
  AeroSniffer.ino          — Orchestration: setup(), Core 0/1 tasks
  AeroSnifferOS.h/.cpp     — Shared types: EventBus, CreatureState, FaceEngine
  Config.h                 — Pins, thresholds, feature flags
  AttentionEngine.cpp      — Event → attention mapping, decay
  MoodEngine.cpp           — Mood arbitration, fixed-point decay
  PersistenceService.cpp   — NVS-backed CreatureProfile
  MemoryEngine.cpp         — Memory formation, decay, recall
  Companion/               — AttentionEngine.h, MoodEngine.h
  Memory/                  — MemoryTypes.h, MemoryEngine.h
  Security/                — Portal UI, threat detection
  Mode1_Pet.h              — Companion mode logic
  Mode2_Security.h         — Security mode logic
  Mode3_Aviation.h         — Aviation mode logic
```

## Architecture Rules

This project follows a strict layered pipeline:

```
Observe → Attention → Emotion → Mood → Face
                                           ↑
                                    Persistence (observer)
                                    Memory (observer)
```

- **No layer reads or writes upstream fields.** Persistence and Memory observe
  state but never feed back into earlier stages.
- **No EventBus subscriptions in render code.** Mode tasks may publish events;
  subsystems subscribe in `setup()`.
- **No spinlocks outside AttentionEngine.** The EventBus is synchronous and
  Core 1 only. Cross-core data uses `volatile` flags and atomic increments.
- **Observer pattern for new subsystems.** Read `g_creature`, don't write to it.

## Frozen Subsystems

V2.5 (Creature Brain) and V2.6 (Memory & Release Engineering) are certified and frozen. No further tuning.
All development occurs on `main` with sprint-level commits.

## Licensing

By contributing, you agree that your contributions will be licensed under the
project's existing license.
