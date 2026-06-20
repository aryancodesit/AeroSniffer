# Project Status — AeroSniffer V2.5

## Phase: Creature Brain

### Completed

| Sprint | Description | Status |
|--------|-------------|--------|
| **Sprint 1** | Companion Intelligence Foundation | ✅ Committed, hardware-validated |
| **Sprint 2A** | FaceEngine Attention Migration | ✅ Committed, hardware-validated |

### In Progress

| Sprint | Description | Status |
|--------|-------------|--------|
| — | Next V2.5 milestone | Planning |

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

### Recent Commits

| Date | Commit | Description |
|------|--------|-------------|
| 2026-06-20 | `c3a6480` | Sprint 2A: FaceEngine attention migration |
| 2026-06-20 | `69ea694` | Governance: SECURITY.md, PR template |
| 2026-06-20 | `16db15f` | Sprint 1: Companion intelligence foundation |
| 2026-06-20 | `6a900b2` | Dependabot config for monthly security updates |

### Branch

- Active: `feature/v2.5-creature-brain`
- Upstream: `origin/feature/v2.5-creature-brain`
- Base: `main`
