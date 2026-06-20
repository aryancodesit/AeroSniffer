# Security Policy

## Supported Versions

Only the **latest tagged release** receives security patches.

| Version | Supported |
|---------|-----------|
| v2.4    | ✅ Active |
| v2.3    | ❌ End of life |
| < v2.3  | ❌ End of life |

For the current development branch (`feature/*`), security issues are addressed
on a best-effort basis before the next release.

## Reporting a Vulnerability

Report security vulnerabilities by **opening a GitHub Issue** with the
`security` label. Do not use a public issue for active exploits.

If the issue is time-sensitive or involves credentials, email the repository
owner directly (visible on the GitHub profile).

**Do not** post credentials, tokens, or network topology in public issues.

## Disclosure Policy

1. Report is acknowledged within **7 days**.
2. A fix is targeted for the **next minor release** (or earlier for critical
   issues).
3. The reporter is credited in the release notes (unless anonymity is
   requested).

Since this is a **single-maintainer project**, response times are best-effort.
Critical firmware vulnerabilities (OTA bypass, credential leakage, persistent
DoS) are prioritised over dependency advisories.

## Scope

In scope:
- Firmware (`AeroSniffer/`) — ESP32-S3 runtime behaviour
- Companion app (`companion-app/`) — web dashboard
- PC agent (`pc-agent/`) — host telemetry bridge
- Build and deployment tooling (`tools/`)

Out of scope:
- Third-party dependencies (report upstream)
- Modified hardware revisions (DeskBuddy 2.0 only)
- Physical attacks requiring device disassembly
