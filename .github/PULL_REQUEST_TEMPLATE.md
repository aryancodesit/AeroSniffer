## Summary

<!-- One paragraph. What does this change do and why? -->

## Files Changed

<!-- List the files modified, added, or deleted. -->

- `path/to/file` — why

## Hardware Testing

- [ ] Compiled and flashed to ESP32-S3 (DeskBuddy 2.0)
- [ ] Tested in **Pet Mode**
- [ ] Tested in **Security Mode**
- [ ] Tested in **Aviation Mode**
- [ ] Mode transitions (Pet → Security → Aviation → Pet) cycle cleanly
- [ ] Touch input (tap, long press) works in all modes
- [ ] WiFi connect/disconnect does not crash or block rendering

## Risks

<!--
What could go wrong? What's the rollback plan?
If no risks, write "None identified."
-->

- **Runtime**: <!-- e.g. "Increased stack usage by 200 bytes" -->
- **Regression**: <!-- e.g. "Changes gaze logic — verify eye movement" -->
- **Rollback**: <!-- e.g. "Revert commit <hash>" -->

## Checklist

- [ ] Code compiles with `pio run` (or Arduino IDE)
- [ ] No new compiler warnings
- [ ] Existing `[ATTN]`, `[MODE]`, `[FACE]` serial output is preserved
- [ ] `CHANGELOG.md` updated (if user-facing change)
- [ ] `BUG_LOG.md` updated (if fixing a bug)
