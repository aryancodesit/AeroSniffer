# V2.7.3 Calibration Registry

Only constants tagged **Measure** are tracked here. All other constants (Freeze, Observe) are documented in the calibration plan.

| ID | Constant | Location | Current Value | Decision | Evidence Source | Final Value | Status |
|----|----------|----------|---------------|----------|----------------|-------------|--------|
| B1 | `kEngagementDecayPerSec` | `BehaviorEngine.cpp` | `0.333f` | **No Change** | `V2.7.3_B1_001`, `V2.7.3_B1_002`, `V2.7.3_B1_003` | `0.333f` | Complete — repeatable decay, adjusting would calibrate around uint16_t logic defect |
| M8 | PLAYFUL decay interval | `MoodEngine.cpp` | `36000` ms/unit | No Change | Multiple observed runs — decay consistent, no oscillation, no stuck transitions | `36000` ms/unit | Complete — evidence sufficient. 3+ observed decays: Happy→Relaxed normal, no unexpected behavior. Additional runs would not change decision. |
| M9 | ANXIOUS decay interval | `MoodEngine.cpp` | `24000` ms/unit | No Change | Engineering Equivalence — identical decay code path as M8 | `24000` ms/unit | Complete — same decay mechanism. Threat-generation testing would not materially reduce uncertainty. |

## Rules

- Populate **Evidence Source** with `test_id` after Phase 3 analysis completes.
- Populate **Final Value** only if range/mean < 0.10 and mean excludes current value.
- If no adjustment is warranted, set Final Value = Current Value and note "No change."
