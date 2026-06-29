# Engineering Handbook — AeroSniffer

**Version:** 1.1.1 · **Sprint:** 4 (V2.7) · **Supersedes:** v1.1 (2026-06-25)

## Principles

1. **Ship working firmware.** Process exists to protect this, not to create work.
2. **Evidence over assertion.** Every certification claim cites a hardware log line or a code path match.
3. **Scale process to team size.** Rules must not break when the team grows, but must not impose team-scale overhead on a single person.
4. **Fix process bugs the same way you fix firmware bugs.** If a rule causes more friction than it prevents, change the rule.

---

This handbook uses three tiers. STOP rules prevent the device from crashing or losing data. SLOW rules increase risk when skipped — know why you are skipping them. GROW rules are dormant until the team expands.

---

## STOP Rules

Violating any STOP rule means the release does not ship until it is fixed.

### S1. Pipeline freeze
The top-level tick order (Attention → Emotion → Mood → Persistence → Memory) is frozen. An ADR (see L2) is required to unfreeze or reorder it.

### S2. No dynamic allocation in engine code
No dynamic allocation in engine code. The full rule with exceptions is defined in CODING STANDARDS (§4).

### S3. Evidence citation for certification claims
Every feature or subtype claimed in a release must cite one of:
- **Hardware**: `[LOG] path:line` — specific log line from a hardware run
- **Static analysis**: `[SA] <code path match>` — which proven subtype shares this code path, and what differs

No uncited claims. This is non-negotiable because it is the only thing preventing fictional release notes.

### S4. Release gate checklist
Run this before `git tag`. Every item must pass.

```
[ ] git status --short returns clean
    (untracked .md files may be excluded by documented decision)
[ ] Firmware compiles with zero errors
[ ] Every feature claim cites hardware or static analysis evidence (S3)
[ ] Soak evidence exists if tick/decay/ring-buffer changed (see S5)
[ ] CHANGELOG.md updated for this sprint
[ ] Tag message includes what changed and known bugs
[ ] All release candidate tags deleted
[ ] .gitattributes present with * text=auto
[ ] ADR written for any architecture change this sprint (see L2)
```

For emergency hotfixes (CRITICAL bug found post-release), the gate reduces to: compile check, fix evidence cited, BUG_LOG updated. Cleanup follows the standard post-release schedule (see L7).

### S5. Soak requirement
Any sprint modifying the tick path, ring buffer, or decay pipeline requires a **30-minute minimum hardware soak** demonstrating:
- Continuous heartbeats
- Zero WDT resets, panics, or fatal errors
- Record count monotonically non-decreasing

This is a P3 evidence requirement (see SLOW rules).

### S6. Review killers
Six violations defined in CODING STANDARDS (§11) that stop a commit regardless of correctness.

---

## SLOW Rules

Skipping these increases risk. Document why when you skip them.

### L1. Code review
Engine `.cpp` and `.h` changes must be reviewed. Solo: self-review via commit-body justification (what changed, why, alternatives considered). Team (2+ people): PR review with approval from a different person. Markdown, `.gitignore`, `.gitattributes`, and experimental reverts within the same session are not subject to review.

### L2. ADR for architecture changes
Write an ADR before implementing any of:
- Changing the frozen pipeline order
- Adding or removing an engine
- Changing tick execution order
- Deprecating or removing a feature
- Changing the hardware target
- Changing the persistence format (NVS schema)
- Changing the serial protocol (RES:/EVT: format)

ADR format: Context, Decision, Consequences, Evidence. Sequential numbering (`ADR-NNN.md`), stored in `docs/ADR/`. Status transitions: Proposed (drafted), Accepted (implementation started), Deprecated (superseded by new ADR). Never delete an ADR.

An ADR is not required for experimental or exploratory work on a branch. It is required before the change merges to main.

### L3. Cross-engine timing review
Any sprint adding an engine that reads state produced by another engine in the same tick must document the tick order and trace two edge cases:
1. First tick after boot
2. Long-duration boundary (≥30 min for mood, ≥60 min for decay)

### L4. Test harness requirement
Before a feature depending on external stimulus (network attack, sensor threshold, rare flight) enters a sprint, the test harness that can generate that stimulus must exist. Three options: real stimulus, software injector (serial command that publishes the equivalent EventBus event), or static analysis (documented code-path match to a hardware-proven subtype).

### L5. Evidence levels
| Level | Applies To | Evidence Required | Example |
|-------|-----------|------------------|---------|
| P1 — Unit | Single function or module | Static analysis or one log line | Dedup window boundaries |
| P2 — Integration | Cross-module interaction | Tick diagram + hardware run | Touch → Memory formation |
| P3 — Soak | Long-duration stability | 30-min hardware log | 83-minute continuous run |

Every feature claimed at release must have P1, P2, or P3 evidence. One regression test from the prior sprint must be re-run and produce the same result. If no prior test exercises this sprint's changed code, design a new regression test.

### L6. Arch diagram accuracy
Architecture diagrams (`ARCHITECTURE.md`, `WIRING.md`) are reviewed for accuracy during the quarterly repository health review. In-sprint updates are encouraged but not required.

### L7. Post-release cleanup
- Delete release candidate tags
- Delete merged feature branches (remote + local)
- Delete release branch from the prior release
- Compress raw test logs >1 MB
- Archive evidence summary in sprint results doc

Complete all items before the next sprint's release gate.

### L8. Bug tracking
CRITICAL and HIGH bugs get a `BUG_LOG.md` entry with severity, symptoms, reproduction steps, and status. MEDIUM and LOW bugs can be documented with a commit message reference. Fix-and-forget bugs (discovered and fixed in the same session) do not need a separate BUG_LOG entry.

### L9. Technical debt
Debt items are tracked in `docs/governance/TECH_DEBT.md`. Each item has a rationale for deferral and a trigger condition for fixing. Spend half a day per sprint on debt. If a debt item causes a bug or a 30-minute debugging session, escalate its severity and schedule it — do not defer it again.

---

## GROW Rules

These are dormant. They activate when a second developer opens a PR against the repository. Ignore them until then.

- Feature branches for all changes (direct commit to main for solo is fine)
- PR review checklist template
- Full review concerns table (resource impact, error handling, timing, side effects)
- Release branch per version (for hotfix isolation)
- Weekly 15-minute cross-engine sync
- Debt sprint allocation (every Nth sprint)
- Docs restructure (move sprint files to `docs/sprints/`)
- EventBus event registry table
- Serial protocol documentation in `docs/protocol/`
- GitHub Releases with binaries
- GitHub Issues for bug tracking (migrate from BUG_LOG.md)

---

## Quick Reference (Solo)

1. Commit directly to main. Use branches for experiments.
2. Commit messages: what changed, why, alternatives considered.
3. Cite evidence for every certification claim. One log line beats reasoning.
4. Write an ADR before changing pipeline order or adding engines.
5. Run the release gate checklist before tagging. Do not skip steps.
6. Delete release candidate tags after the release tag is created.
7. Run one regression test from the prior sprint.
8. Spend half a day per sprint on technical debt.
9. Post-release cleanup before the next release gate.
10. If a rule causes friction, change the rule (principle 4).

---

## Comparison to Professional Practice

| Practice | Pro Team | AeroSniffer | Rationale |
|----------|---------|-------------|-----------|
| Code review | Required per change | Solo: self-review; Team: PR review | 80% of the value at 10% of the overhead for solo |
| CI build | Required per commit | **Missing — add before Sprint 4** | Catches broken builds before hardware flash |
| Unit tests | Required | Not required | ESP32 lacks host-testable build target |
| Feature branches | Required | Solo: direct commit; Team: branches | Branch overhead exceeds benefit for solo |
| Issue tracking | JIRA / GH Issues | BUG_LOG.md | Scales to solo; migrate to GH Issues V2.0 |
| Regression suite | Automated | One prior-sprint test | Bounded growth: V2.10 has 5 tests |
| ADR | Varies by team | Required for architecture changes | Lightweight, prevents architectural amnesia |
| Release gates | Multi-stage signoff | Single checklist | Checklist replaces absent approvers |
| Debt management | 20% sprint capacity | Half-day per sprint | Achievable at solo scale |
