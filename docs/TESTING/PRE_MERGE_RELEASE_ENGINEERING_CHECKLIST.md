# Pre-Merge Release Engineering Checklist

**Branch:** `feature/v2.6-releng-validation` → `main`  
**Date:** 2026-06-28  
**Commit:** `ac8bb92`  
**Auditor:** OpenCode Release Engineering Agent

---

## Required Fixes (Blocking Merge)

| # | File | Issue | Fix |
|---|------|-------|-----|
| 1 | `.github/workflows/firmware-build.yml:5` | **Validation branch in push trigger** — `feature/v2.6-releng-validation` is listed alongside `main`. This is a remnant of the certification phase. | Remove `, feature/v2.6-releng-validation` from line 5. The PR trigger (`pull_request → main`) handles future validation. |

**Fix:**
```yaml
# Before:
    branches: [main, feature/v2.6-releng-validation]

# After:
    branches: [main]
```

## Recommended Before First Release Tag

| # | File | Issue | Risk | Fix |
|---|------|-------|------|-----|
| 2 | `.github/workflows/firmware-build.yml:31-36` | **No `restore-keys` on arduino cache** — `hashFiles('tools/deps.toml')` is an exact match. Any change to `deps.toml` (core version, lib version, new dep) invalidates the cache entirely. The full 1.5 GB must be re-downloaded and re-cached. | **Low** — Cache miss adds ~30s to build step. Acceptable during active development. | Add `restore-keys: \|` below line 36 with `arduino15-ci-` as a fallback. |
| 3 | `.github/workflows/firmware-build.yml` (before line 18) | **No explicit `permissions` block** — Release creation (`ncipollo/release-action@v1`) requires `contents: write` on the GITHUB_TOKEN. Default token behavior varies by org settings; on restricted orgs, the release step will fail. | **Low** — Works on public repo with default settings. Fails only if org explicitly restricts GITHUB_TOKEN permissions. | Add `permissions: contents: write` at job level. |
| 4 | `.github/workflows/firmware-build.yml:75` | **Release body is full `changelog.md`** — The `bodyFile` parameter passes the **entire** changelog to every release. For the first tag (`v2.6.0`), the entire file is fine. For subsequent tags, every release will contain the full history, not a per-version summary. | **Low** — Only matters for multi-release projects. Acceptable for initial v2.6.0 release. | Before tagging v2.6.1+, extract version-specific section from `changelog.md`. |

## Verification Checklist (Pre-Merge)

- [ ] **Line 5**: `branches: [main]` only — no feature branch in push trigger
- [ ] **Line 10-11**: `pull_request: branches: [main]` — PR trigger is active
- [ ] **Line 15**: `ARDUINO_CLI_VERSION: "1.5.1"` — correct version
- [ ] **Line 23**: `fetch-depth: 0` — full history for `git describe`
- [ ] **Line 28-29**: `cache: pip` with `cache-dependency-path: tools/requirements.txt`
- [ ] **Line 36**: `key: arduino15-ci-${{ hashFiles('tools/deps.toml') }}` — cache key is correct
- [ ] **Line 41**: `arduino/setup-arduino-cli@v2` — v2, not v1
- [ ] **Line 48**: `python3 tools/releng.py ci-build` — single entry point
- [ ] **Line 49**: `VERSION=... >> "$GITHUB_OUTPUT"` — version capture for artifact naming
- [ ] **Lines 52, 63**: `if: always()` — uploads execute even on failure
- [ ] **Lines 56-59**: Upload paths match releng.py output (`build/*.bin`, `build/checksums.sha256`, `build/build-info.yaml`)
- [ ] **Lines 66-67**: Evidence upload path matches evidence.py output (`build/evidence/`)
- [ ] **Line 71**: `if: startsWith(github.ref, 'refs/tags/')` — release only on tag
- [ ] **Line 74**: Release artifacts match upload paths
- [ ] **`RELEASE_ENGINEERING_CERTIFICATION.md`**: Decide whether to include in merge commit or discard (it documents the certification process, not the production subsystem)

## Git Hygiene

The commit log on `feature/v2.6-releng-validation` vs `origin/main`:

```
ac8bb92 revert: remove #error injection and warm-cache trigger
dbc11fd test: inject #error for failure-path validation          ← validation only
26b5101 chore: warm-cache trigger (no-op comment)                 ← validation only
964c2fc fix: upgrade arduino/setup-arduino-cli@v1->@v2           ← production fix
2cc1da6 ci: add feature/v2.6-releng-validation to push trigger    ← validation only
70891ca release(v2.6): CI pipeline, evidence framework, ...       ← production commit
```

**Recommendation:** Squash the three validation-only commits (`2cc1da6`, `26b5101`, `dbc11fd`) into `70891ca`. The revert (`ac8bb92`) makes them net-zero anyway. After squashing, the log is:

```
964c2fc fix: upgrade arduino/setup-arduino-cli@v1->@v2
70891ca release(v2.6): CI pipeline, evidence framework, ...
```

Alternatively, keep all commits for audit trail — the validation history is harmless.

## Pipeline Health Summary

| Metric | Value |
|--------|-------|
| Total runs this session | 5 (15, 20, 21, 22, 23) |
| Successful runs | 4 (20, 21, 22, 23) |
| Cold build | 114s |
| Warm build | 87-101s |
| Failure-path build | 56s (build_success=false, evidence uploaded) |
| Tests | 26/26 passing |
| Evidence providers | 5 (arduino, compiler, git, linker, tft) |
| Workflow lines | 77 (target: <100) |

---

*Generated by OpenCode Release Engineering Agent. Items marked "Required" must be addressed before merging to `main`. Items marked "Recommended" should be addressed before the first production release tag.*
