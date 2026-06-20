# AeroSniffer — Copyright Header Templates

Every new source file in the AeroSniffer repository should carry
a copyright header matching its language convention.

Use the templates below when creating new files.

Existing files may be updated during a dedicated header-audit pass.
Do not modify source files during active feature work.

---

## C/C++ Headers (\*.cpp, \*.h, \*.ino)

```cpp
// ================================================================
//  filename.ext  —  Brief Description
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  Copyright (c) 2026 AeroSniffer Project
//  SPDX-License-Identifier: MIT
// ================================================================
```

**Example** (new file):

```cpp
// ================================================================
//  CompanionBrain.h  —  Drive System & Behavior Selector
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  Copyright (c) 2026 AeroSniffer Project
//  SPDX-License-Identifier: MIT
// ================================================================
```

### Placement
- Line 1: opening rule
- Lines 2–6: header content
- Line 7 (if needed): blank line before `#pragma once` or `#include`

---

## Python Scripts (\*.py)

```python
#!/usr/bin/env python3
"""
AeroSniffer — filename.ext

Brief description of what this script does.

Copyright (c) 2026 AeroSniffer Project
SPDX-License-Identifier: MIT
"""
```

**Example** (new file):

```python
#!/usr/bin/env python3
"""
AeroSniffer — companion_state.py

Aggregates and forwards companion telemetry from the serial daemon.

Copyright (c) 2026 AeroSniffer Project
SPDX-License-Identifier: MIT
"""
```

### Placement
- Line 1: `#!/usr/bin/env python3` (only for entry-point scripts)
- Lines 2–end: module docstring with description, copyright, and SPDX
- Followed by imports

---

## TypeScript / TSX (\*.ts, \*.tsx)

```tsx
/**
 * AeroSniffer — filename.ext
 *
 * Brief description of what this module exports.
 *
 * Copyright (c) 2026 AeroSniffer Project
 * SPDX-License-Identifier: MIT
 */
```

**Example** (new file):

```tsx
/**
 * AeroSniffer — BotFace.tsx
 *
 * Animated companion face component with emotion-state mapping.
 *
 * Copyright (c) 2026 AeroSniffer Project
 * SPDX-License-Identifier: MIT
 */
```

### Placement
- Top of file, before any imports
- JSDoc-style block comment (`/** ... */`)

---

## SPDX Identifier

All headers should include the SPDX license identifier `SPDX-License-Identifier: MIT`
to enable automated license compliance scanning.

## Year Convention

Use the year the file was **created**. When a file is significantly modified
in a later year, append the later year:

```
Copyright (c) 2026 AeroSniffer Project
Copyright (c) 2026, 2027 AeroSniffer Project
```

Do not use year ranges (e.g. `2026-2027`). List each year individually if
the file has been modified across multiple years.
