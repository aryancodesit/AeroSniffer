# Archived Planning Documents

This directory contains obsolete planning documents preserved for historical reference. These documents describe design rationale, release criteria, and feature prioritization from earlier versions of AeroSniffer.

## Contents

| File | Original Location | Vintage | Historical Value |
|------|-------------------|---------|------------------|
| `release_plan.md` | Root | V2.5 | Milestone A-E planning, stabilization entry criteria |
| `stabilization_sprint.md` | Root | V2.5 | BUG-007 investigation context |
| `feature_backlog.md` | Root | V2.5 | Feature prioritization rationale for V2.5 era |
| `RC1_VALIDATION_PLAN.md` | Root | V2.3 | Full V2.3 validation matrix (useful as template for future validation) |
| `roadmap.md` | Root (was `docs/ROADMAP.md` V2.3) | V2.5 | V2.5 stabilization roadmap. V2.3 `docs/ROADMAP.md` content was lost during archival due to case-insensitive filesystem collision. |

## Removed (No Historical Value)

| File | Reason |
|------|--------|
| `docs/CHANGELOG.md` | Stub file — superseded by root `changelog.md` |
| `docs/ANCHORED_SUMMARY.md` | Session artifact — no project value |

## Current Source of Truth

- **Project status**: `PROJECT_STATUS.md` (root)
- **Architecture**: `docs/ARCHITECTURE.md`
- **Bug tracking**: `BUG_LOG.md` (root)
- **Changelog**: `changelog.md` (root)
- **Roadmap**: `roadmap.md` (root — consolidated)

## Restoration

To restore any file to its original location:
```
git mv docs/archive/<file> <original-location>
```
