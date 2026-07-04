# V2.9 Face Bible — AeroSniffer Identity Reference

> Frozen art direction. No further face changes after v2.9.0.

## Feature Classification System

### Eye Openness
```
0%      25%     50%     75%     100%
▁       ▂       ▄       ▆       █
Closed  Sleepy  Normal  Open    Wide
h~4     h~14    h~24    h~28    h~34
```

### Roundness
```
Very Round     Round      Neutral    Narrow      Sharp
◯             ○           ◡          ⌿           △
WIDE          ROUND      DEFAULT    FLAT        SQUINT
```

### Brow Energy
```
Relaxed     Lifted      Concerned   Focused    Aggressive
—           ╱           ╲           ╱╲         ╲╱
NORMAL      RAISED      WORRIED     RAISED*    FURROWED
```

### Pupil Size
```
None        Half        Normal      Dilated    Constricted
··          ──          ●           ◉          ·
h=0         h/2         r=3         r=5        r=2
```

### Mouth Energy
```
Flat        Hint        Smile       Wide       Laugh
—           ⌣          ◠           ◠◠         ○
rect        arc         circle      big circ   O-shape
```

---

## Emotion Silhouette Reference

```
CALM         HAPPY        CURIOUS      ALERT
○   ○        ◡   ◡        ◡  ◡         ○   ○
  ◡            ◠           ‿            —

EXCITED      SLEEPY       SAD          SURPRISED    LOVE
◕   ◕       •   •       ○   ○        ○   ○        ◡   ◡
  ◠           —           ⌢            ○            ◠
```

---

## Emotion Specification Table

### CALM
```
Openness │ 50% (h~24)
Roundness│ Round (PRES_EYE_ROUND)
Brows    │ Relaxed (PRES_BROW_NORMAL)
Pupils   │ Normal (r=3)
Mouth    │ Hint (arc)
Motion   │ Low bounce, slow breath
```
```
○     ○
  ◡
```

### HAPPY
```
Openness │ 50% (h~24)
Roundness│ Arched (PRES_EYE_ARCHED)
Brows    │ Lifted (PRES_BROW_RAISED)
Pupils   │ Slightly dilated (r=4)
Mouth    │ Smile (circle)
Motion   │ Medium bounce
```
```
◠     ◠
  ◠
```

### CURIOUS
```
Openness │ 60% (h~26)
Roundness│ Round (PRES_EYE_ROUND)
Brows    │ Lifted (PRES_BROW_RAISED) / Asymmetric (PRES_BROW_ONE)
Pupils   │ Dilated (r=5)
Mouth    │ Tiny smile
Motion   │ Head tilt, medium saccade
```
```
◠     ◡
  ·
```

### ALERT
```
Openness │ 75% (h~30)
Roundness│ Round → Wide
Brows    │ Lifted (PRES_BROW_RAISED) — soft, not furrowed
Pupils   │ Dilated (r=5)
Mouth    │ Neutral
Motion   │ High, quick saccade, slight lift
```
```
○   ○
  —
Intent: "I heard something."
```

### EXCITED
```
Openness │ 90% (h~32)
Roundness│ Very Round (PRES_EYE_WIDE)
Brows    │ Lifted (PRES_BROW_RAISED)
Pupils   │ Dilated (r=5)
Mouth    │ Wide smile
Motion   │ High bounce, fast everything
```
```
◕   ◕
  ◠
Intent: "Overwhelming joy."
```

### SLEEPY
```
Openness │ 25% (h~14)
Roundness│ Flat (PRES_EYE_FLAT)
Brows    │ Relaxed (PRES_BROW_NORMAL)
Pupils   │ Half / None
Mouth    │ Flat
Motion   │ Minimal
```
```
•   •
  —
```

### SAD
```
Openness │ 40% (h~20)
Roundness│ Round (PRES_EYE_ROUND)
Brows    │ Concerned (PRES_BROW_WORRIED)
Pupils   │ Constricted (r=2)
Mouth    │ Frown
Motion   │ Low
```
```
○   ○
  ⌢
```

### SURPRISED
```
Openness │ 100% (h~34)
Roundness│ Very Round (PRES_EYE_WIDE)
Brows    │ Lifted (PRES_BROW_RAISED)
Pupils   │ Dilated (r=5)
Mouth    │ O-shape
Motion   │ High, freeze first then react
```
```
○   ○
  ○
```

### LOVE
```
Openness │ 50% (h~24)
Roundness│ Arched (PRES_EYE_ARCHED)
Brows    │ Relaxed (PRES_BROW_NORMAL)
Pupils   │ Dilated (r=5)
Mouth    │ Warm smile
Motion   │ Medium, gentle
```
```
◡   ◡
  ◠
```

### ANGRY
```
Openness │ 30% (h~18)
Roundness│ Sharp (PRES_EYE_SQUINT)
Brows    │ Aggressive (PRES_BROW_FURROWED)
Pupils   │ Constricted (r=2)
Mouth    │ Flat line
Motion   │ Rigid, tense
```
```
△   △
  —
Reserved sharpness. Only negative emotion with aggressive geometry.
```

---

## Design Rules (Frozen)

1. **Sharpness is earned.** Only ANGRY uses aggressive geometry (SQUINT triangle, FURROWED sharp brows). All other emotions use rounded shapes.
2. **Pupils are always circular.** Every emotion, every variant. No rectangular pupils.
3. **Same base silhouette.** The creature should look like itself in every emotion — only proportions and features shift.
4. **Excited = bigger, not sharper.** Wider eyes, bigger smile, more bounce. No spiky geometry.
5. **Alert = noticing, not hostile.** Wide round eyes, soft lifted brows. No furrowed or sharp elements. Communicates attention, not aggression.
6. **Curious SQUINT uses a soft capsule** (not the aggressive triangle) when SQUINT is selected. ANGRY keeps the sharp triangle SQUINT.
7. **Maximum angularity = maximum tension.** ANGRY is the only emotion that earns sharp corners.
8. **Motion hides geometry.** Expression sequencing (mouth→eyes→brows) matters more than any individual primitive.

## Variant Audit

### CALM (3 variants)
| V | Eye | Brow | Pupil | Status |
|---|---|---|---|---|
| 0 | ROUND | NORMAL | NORMAL | ✅ |
| 1 | ROUND | NORMAL | HALF | ✅ |
| 2 | ROUND | NORMAL | NORMAL | ✅ |

### HAPPY (3 variants)
| V | Eye | Brow | Pupil | Status |
|---|---|---|---|---|
| 0 | ARCHED | NORMAL | NORMAL → CIRCULAR | ✅ Need pupil fix |
| 1 | WIDE | RAISED | DILATED | ✅ |
| 2 | ARCHED | NORMAL | HALF | ✅ |

### CURIOUS (3 variants)
| V | Eye | Brow | Pupil | Status |
|---|---|---|---|---|
| 0 | ROUND | RAISED | DILATED | ✅ |
| 1 | SQUINT | NORMAL | CONSTRICTED | 🟡 Needs soft capsule, not triangle |
| 2 | WIDE | ONE | DILATED | ✅ |

### ALERT (3 variants)
| V | Eye | Brow | Pupil | Status |
|---|---|---|---|---|
| 0 | WIDE | RAISED | DILATED | ✅ |
| 1 | ROUND | FURROWED | NORMAL → DILATED | 🟡 Brow hostility, pupil too small |
| 2 | DEFAULT | NORMAL | NORMAL | ✅ Calm sub-variant |

### EXCITED (3 variants)
| V | Eye | Brow | Pupil | Anim | Status |
|---|---|---|---|---|---|
| 0 | WIDE | RAISED | DILATED | 5 | ✅ |
| 1 | ROUND | NORMAL | NORMAL → DILATED | 1 → 5 | 🟡 Needs more energy |
| 2 | ARCHED → ROUND | RAISED | DILATED | 5 | 🟡 ARCHED too sharp, use ROUND |

### SLEEPY (3 variants)
| V | Eye | Brow | Pupil | Status |
|---|---|---|---|---|
| 0 | FLAT | NORMAL | HALF | ✅ |
| 1 | FLAT | NORMAL | NONE | ✅ |
| 2 | ROUND | NORMAL | NORMAL | ✅ |

### SAD (3 variants)
| V | Eye | Brow | Pupil | Status |
|---|---|---|---|---|
| 0 | ROUND | WORRIED | CONSTRICTED | ✅ |
| 1 | FLAT | NORMAL | HALF | ✅ |
| 2 | ROUND | NORMAL | NORMAL | ✅ |

### SURPRISED (3 variants)
| V | Eye | Brow | Pupil | Status |
|---|---|---|---|---|
| 0 | WIDE | RAISED | DILATED | ✅ |
| 1 | ROUND | RAISED | CONSTRICTED | ✅ |
| 2 | ROUND | NORMAL | NORMAL | ✅ |

### LOVE (3 variants)
| V | Eye | Brow | Pupil | Status |
|---|---|---|---|---|
| 0 | ARCHED | NORMAL | DILATED | ✅ Need pupil fix |
| 1 | ROUND | NORMAL | NORMAL | ✅ |
| 2 | WIDE | RAISED | DILATED | ✅ |

### ANGRY (3 variants)
| V | Eye | Brow | Pupil | Status |
|---|---|---|---|---|
| 0 | SQUINT | FURROWED | CONSTRICTED | ✅ Keeps sharpness |
| 1 | ROUND | FURROWED | NORMAL | ✅ |
| 2 | ROUND | NORMAL | CONSTRICTED | ✅ |

## Frozen: No further face changes after v2.9.0
