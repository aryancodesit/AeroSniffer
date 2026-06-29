# Quarterly Repository Health Review

Run once per quarter (every 3 months). Duration: ~30 minutes for solo, ~1 hour for team of 3–5.

---

## Checklist

### 1. Branch Cleanup

```
[ ] All merged feature branches deleted (remote + local)
[ ] Stale branches (>90 days without activity) either deleted or tagged as ARCHIVED
[ ] No branch named after a closed sprint (e.g. `feature/v2.6-memory-expansion`)
```

Target: only active branches and `main` remain.

---

### 2. Tag Cleanup

```
[ ] Release candidate tags from the last two releases deleted
[ ] Checkpoint/intermediate tags deleted
[ ] Stale tags not matching the release convention (v2.X) pruned
```

Run: `git tag -l` and inspect for anything that is not a release tag.

---

### 3. CI Health

(Defer if CI is not yet set up.)

```
[ ] Last 10 CI runs on main are passing
[ ] No workflow file is pinned to a commit SHA that is >6 months old
[ ] Average CI time is below 5 minutes
[ ] No disabled or broken workflows
```

---

### 4. Dependency Audit

```
[ ] Dependabot (or equivalent) is configured and running
[ ] No dependency update PRs older than 30 days
[ ] Subproject dependencies (companion-app, pc-agent, tools) batched weekly
[ ] No pinned dependency with a known CVE (check GitHub Security Advisories)
```

---

### 5. ADR Review

```
[ ] All ADRs in docs/governance/ADR/ have correct Status field
[ ] No ACCEPTED ADR that contradicts current architecture
[ ] No PROPOSED ADR older than 2 quarters (either accept or close)
```

---

### 6. Documentation Freshness

```
[ ] CHANGELOG.md covers the last two releases
[ ] AI_HANDOFF.md mentions current sprint correctly
[ ] PROJECT_STATUS.md is not empty (one-liner is sufficient)
[ ] BUG_LOG.md reviewed — no FIXED bugs remaining in OPEN section
```

---

### 7. Tech Debt Review

```
[ ] TECH_DEBT.md reviewed — no deferred item has escalated in severity
[ ] Any debt item older than 4 quarters either scheduled or marked WONT_FIX
[ ] No new debt introduced this quarter that was not logged
```

---

### 8. Issue Triage

```
[ ] All CRITICAL bugs in BUG_LOG.md are FIXED or have a scheduled sprint
[ ] No HIGH bug is older than 1 quarter without a resolution plan
[ ] Stale bug entries (>1 year) reviewed and either closed or refreshed
```

---

### 9. Release Checklist Review

```
[ ] Last release gate checklist was completed (no skipped steps)
[ ] Post-release cleanup from last release was done
[ ] Sprint results from last two sprints are in docs/SPRINTS/
```

---

### 10. Governance Self-Check

```
[ ] Handbook reflects current practices (update if sprint process changed)
[ ] Coding standards are up to date with actual codebase conventions
[ ] This review itself is documented (date + findings)
```

---

## Review Record

| Quarter | Date | Reviewer | Issues Found | Actions Taken |
|---------|------|----------|-------------|---------------|
| 2026-Q2 | | | | |
| 2026-Q3 | | | | |
| 2026-Q4 | | | | |
| 2027-Q1 | | | | |

---

## Scaling Notes

**Solo**: Complete all 10 sections. Estimated time: 30 minutes. Each box gets a checkmark or a note.

**3–5 person team**: Assign sections 1–3 to one person, 4–6 to another, 7–10 to a third. Review together for 15 minutes. Total time: 1 hour.

**Automation target**: Sections 1, 2, 3, and 4 can be fully automated as a GitHub Actions workflow. Section 9 can be partially automated (check that last release tag exists with a message). Sections 5–8 and 10 require human judgment.
