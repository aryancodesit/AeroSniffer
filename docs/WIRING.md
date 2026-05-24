# 🔌 Wiring Guide — AeroSniffer (DeskBuddy 2.0 Kit)

Complete pin-by-pin connection reference for the DeskBuddy 2.0 build.

> **Golden rule:** All modules run on **3.3V** from the XIAO ESP32S3's 3V3 pin.
> The DeskBuddy 2.0 kit comes pre-matched — most wiring is straightforward.

---

## XIAO ESP32S3 — Pin Overview

```
                    ┌──────────────────────────┐
                    │   Seeed XIAO ESP32S3      │
         [3V3] ────┤ 3V3              5V  VBUS ├───── [5V from USB]
         [GND] ────┤ GND             GND       │
                   │                            │
  Touch Sensor ────┤ D0 (GPIO1)                 │
      (free) ──────┤ D1 (GPIO2)                 │
      TFT DC ──────┤ D2 (GPIO3)                 │
      TFT CS ──────┤ D3 (GPIO4)                 │
     (free) ───────┤ D4 (GPIO5)     D10 (GPIO9) ├───── TFT MOSI
     (free) ───────┤ D5 (GPIO6)      D9 (GPIO8) ├───── TFT RST
     (free) ───────┤ D6 (GPIO43)     D8 (GPIO7) ├───── TFT SCLK
     (free) ───────┤ D7 (GPIO44)                │
                    └──────────────────────────┘
```

---

## Module 1 — ST7789 1.3" IPS Display (240×240)

The display connects over 4-wire hardware SPI.

| Display Pin | Label | Wire to XIAO | GPIO # | Notes |
|-------------|-------|-------------|--------|-------|
| VCC | Power | 3V3 | — | |
| GND | Ground | GND | — | |
| SCL / SCK | SPI Clock | D8 | GPIO 7 | |
| SDA / MOSI | SPI Data | D10 | GPIO 9 | |
| RES / RST | Reset | D9 | GPIO 8 | |
| DC / RS | Data/Command | D2 | GPIO 3 | |
| CS | Chip Select | D3 | GPIO 4 | |
| BLK / BL | Backlight | — | — | Wired to VCC on DeskBuddy carrier (always on) |

> 🟡 Some display boards label pins differently. Match by **function**, not label text.

> ⚠️ The backlight is wired to VCC on the DeskBuddy 2.0 carrier board. No GPIO is needed for BL control — it's always on.

---

## Module 2 — Capacitive Touch Sensor (Red Module)

The kit includes a red capacitive touch switch module. It outputs a digital signal.

| Touch Module Pin | Wire to XIAO | GPIO # | Notes |
|-----------------|-------------|--------|-------|
| VCC | 3V3 | — | |
| GND | GND | — | |
| SIG / OUT | D0 | GPIO 1 | Active LOW (LOW when touched) |

### Interaction Behavior

| Gesture | Duration | Action |
|---------|----------|--------|
| Short tap | < 1.5 seconds | Pet interaction (Mode 1), context action (other modes) |
| Long press | ≥ 1.5 seconds | Switch to next mode |

### Placement

Mount the touch module **under the roof** of the 3D-printed enclosure. The capacitive sensing works through thin plastic (2-3mm). Position it so tapping the top of the shell registers cleanly as a "head-pat."

---

## Complete Wire Count Summary

| Module | Wires Needed |
|--------|-------------|
| ST7789 Display | 7 wires (VCC, GND, SCK, MOSI, RST, DC, CS) |
| Capacitive Touch | 3 wires (VCC, GND, SIG) |
| **Total** | **10 wires** |

> 🎉 That's it! Only 10 wires total — compare this to the 27+ wires in the original DevKitC build.

---

## Power Budget

| Module | Current Draw |
|--------|-------------|
| XIAO ESP32S3 core | ~150 mA |
| ST7789 TFT (with backlight) | ~80 mA |
| Capacitive touch module | ~5 mA |
| WiFi active (Mode 2/3) | +80 mA |
| **Total peak** | **~315 mA** |

> ✅ Well within the XIAO's USB power budget and the included LiPo battery's discharge capability. Expect **2-4 hours** of battery life depending on WiFi usage.

---

## Assembly Tips

### Step 1: Wire the display
Connect the 7 SPI wires from the ST7789 module to the XIAO. Use short Dupont jumper wires (female-to-female) or solder directly for a permanent build.

### Step 2: Wire the touch sensor
Connect the 3 wires from the red capacitive touch module to VCC, GND, and D0 on the XIAO.

### Step 3: Test before enclosing
Flash the firmware and verify:
- Splash screen shows on the display
- Short tap triggers pet happy face
- Long press switches modes

### Step 4: Mount in enclosure
- Slide the XIAO into the enclosure with USB-C port accessible
- Press-fit or hot-glue the display into the front window
- Mount the touch sensor under the top shell
- Connect the battery and switch

---

## Common Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| SPI pins swapped | White screen, no display | Double-check MOSI vs SCK — they're on D10 and D8 |
| Touch sensor SIG not connected | No reaction to taps or mode switching | Verify D0 (GPIO 1) is wired to SIG/OUT |
| Wrong board selected in Arduino IDE | Upload fails or boot loop | Select "XIAO_ESP32S3" and enable "USB CDC On Boot" |
| Display upside down | Image is flipped | Change `tft.setRotation(0)` to `tft.setRotation(2)` in `AeroSniffer.ino` |

---

Built with ❤️ on XIAO ESP32S3 | Bhubaneswar, Odisha, India
