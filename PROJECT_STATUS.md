# Project Status — AeroSniffer V2.5

## Phase: Creature Brain

### Completed

| Sprint | Description | Status |
|--------|-------------|--------|
| **Sprint 1** | Companion Intelligence Foundation | ✅ Committed, hardware-validated |
| **Sprint 2A** | FaceEngine Attention Migration | ✅ Committed, hardware-validated |
| **Sprint 2B** | Compatibility Shim Removal | ✅ Committed |
| **Sprint 3A** | Mood Infrastructure | ✅ Committed, hardware-validated |
| **Sprint 3B** | Mood Consumption | ✅ Committed, compile-validated |

### Blocked

None.

### Risks

| Risk | Severity | Mitigation |
|------|----------|------------|
| OpenSky HTTPS fetch failing (B007) | Low | Non-blocking for development. HTTP fallback or cert fix deferred. |
| lwIP assertion on HTTP fetch (B004) | Low | Weather fetch disabled. Fix via hostname resolution before HTTP. |
| `netstack` error on mode transition (B006) | Low | Non-blocking. Mode switch succeeds despite error. |

### Bugs

See [BUG_LOG.md](BUG_LOG.md) for full details.

### Tags

| Tag | Description |
|-----|-------------|
| `v2.5-attention-complete` | Sprint 2B — attention layer stable |
| `v2.5-mood-foundation` | Sprint 3A — mood infrastructure stable |

### Recent Commits

| Date | Commit | Description |
|------|--------|-------------|
| 2026-06-21 | `(HEAD)` | Sprint 3B: Mood consumption |
| 2026-06-21 | `618827a` | Sprint 3A: Mood infrastructure closure |
| 2026-06-20 | `05762ec` | Sprint 2B: Shim removal |
| 2026-06-20 | `c3a6480` | Sprint 2A: FaceEngine attention migration |
| 2026-06-20 | `69ea694` | Governance: SECURITY.md, PR template |
| 2026-06-20 | `16db15f` | Sprint 1: Companion intelligence foundation |
| 2026-06-20 | `6a900b2` | Dependabot config for monthly security updates |

### Branch

- Active: `feature/v2.5-creature-brain`
- Upstream: `origin/feature/v2.5-creature-brain`
- Base: `main`
- Tags: `v2.5-attention-complete`, `v2.5-mood-foundation`

### Next Sprint

**V2.5 Sprint 4 — Memory Layer**

Persistent creature memory (NVS-backed): name, friendship level, interaction history, mood trends, learned preferences. First cross-session state layer.
