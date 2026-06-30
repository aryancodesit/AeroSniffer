# V2.7.3 Calibration Registry

Only constants tagged **Measure** are tracked here. All other constants (Freeze, Observe) are documented in the calibration plan.

| ID | Constant | Location | Current Value | Decision | Evidence Source | Final Value | Status |
|----|----------|----------|---------------|----------|----------------|-------------|--------|
| B1 | `kEngagementDecayPerSec` | `BehaviorEngine.cpp` | `0.333f` | Measure | TBD | TBD | Pending |
| M8 | PLAYFUL decay interval | `MoodEngine.cpp` | `36000` ms/unit | Measure | TBD | TBD | Pending |
| M9 | ANXIOUS decay interval | `MoodEngine.cpp` | `24000` ms/unit | Measure | TBD | TBD | Pending |

## Rules

- Populate **Evidence Source** with `test_id` after Phase 3 analysis completes.
- Populate **Final Value** only if range/mean < 0.10 and mean excludes current value.
- If no adjustment is warranted, set Final Value = Current Value and note "No change."
