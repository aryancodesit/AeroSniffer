# AeroSniffer Release Plan

Last updated: 2026-06-14

## Milestone A: System Stabilization

Goal: Make the current firmware, companion app, and PC agent dependable before feature expansion.

Exit criteria:
- BUG-007 Typing Emotion Stuck is resolved.
- PC Agent emotion ownership is documented and validated.
- Emotion Engine remains stable after PC Agent typing bursts.
- Security Mode remains stable.
- Mode transition stability remains verified.
- Serial commands are verified against firmware behavior.
- Touch short-tap and long-press interactions are validated on hardware.
- Aviation Mode remains verified: Wi-Fi connect, OpenSky login, aircraft retrieval, parsing, and rendering.
- Dashboard connection diagnostics are validated.
- Known bugs are documented with reproduction steps or closure notes.

## Milestone B: Offline Companion Expansion

Status:
Deferred.

Reason:
Deferred until BUG-007 and dashboard connection diagnostics are closed.

## Milestone C: Knowledge & Personality System

Status:
Deferred.

Reason:
Deferred until PC Agent emotion ownership is resolved and runtime validation is complete.

## Milestone D: Advanced Security Features

Status:
Deferred.

Reason:
Deferred until current stabilization items are closed. Security Mode is stable, but no advanced security feature planning should begin during BUG-007 investigation.

## Milestone E: Production Release

Status:
Deferred.

Reason:
Blocked until BUG-007, dashboard connection diagnostics, final transition checks, and release validation are complete.
