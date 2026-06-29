# Architectural Decisions

## ADR-001

Decision:
Device = Observe
Portal = Act
Cloud = Analyze

Status:
Accepted

---

## ADR-002

Decision:
AeroSniffer must remain fully functional standalone.

Status:
Accepted

Rationale:
Portal enhances experience but is not required.

---

## ADR-003

Decision:
Face = Emotion
Text = Information

Status:
Accepted

Rationale:
Avoid creating hundreds of face states.

---

## ADR-004

Decision:
Mobile-first portal design.

Status:
Accepted

Supported:

* Mobile
* Tablet
* Laptop
* Desktop
* Ultrawide
* 4K

---

## ADR-005

Decision:
Event-driven architecture.

Status:
Accepted

Flow:

Sensors
→ Services
→ Event Bus
→ Modes
→ UI

---

## ADR-006

Decision:
Stability before feature expansion.

Status:
Accepted

Priority Order:

1. Stability
2. Documentation
3. Maintainability
4. Features

---

## ADR-0004 (V2.7)

See [`docs/adr/ADR-0004-Behavior-Layer.md`](adr/ADR-0004-Behavior-Layer.md) for the Behavior Layer architecture decision, including:
- Motivation (MemoryEngine has no consumer)
- Architectural boundaries (Memory → Behavior → CreatureState → Face)
- Ownership rules (MoodEngine = baseline, BehaviorEngine = influence, FaceEngine = render)
- Alternatives rejected (Memory→Face direct, ML-based behavior)
- Consequences (positive and negative)
