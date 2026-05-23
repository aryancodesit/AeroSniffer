# 🔌 Wiring Guide — AeroSniffer

Complete pin-by-pin connection reference for all hardware modules.

> **Golden rule:** All modules run on **3.3V** from the ESP32-S3's 3V3 pin.
> Never connect anything directly to 5V (VBUS) unless explicitly stated.

---

## ESP32-S3 DevKitC-1 — Pin Overview

```
                    ┌──────────────────────────┐
                    │     ESP32-S3 DevKitC-1    │
         [3V3] ─────┤ 3V3              5V  VBUS ├───── [5V from USB]
         [GND] ─────┤ GND             GND       ├───── [GND]
                    │                           │
     TFT MOSI ──────┤ GPIO11         GPIO1  SDA ├───── MPU6050 SDA
     TFT SCLK ──────┤ GPIO12         GPIO2  SCL ├───── MPU6050 SCL
       TFT CS ──────┤ GPIO10         GPIO3  ADC ├───── FSR-402
       TFT DC ──────┤ GPIO13         GPIO4   WS ├───── INMP441 WS
      TFT RST ──────┤ GPIO14         GPIO5  SCK ├───── INMP441 SCK
       TFT BL ──────┤ GPIO15         GPIO6   SD ├───── INMP441 SD
                    │                           │
   MAX98357A BCLK ──┤ GPIO7          GPIO0      ├───── BOOT Button (built-in)
    MAX98357A LRC ──┤ GPIO8                     │
    MAX98357A DIN ──┤ GPIO9                     │
                    └──────────────────────────┘
```

---

## Module 1 — TFT Display (ST7789 / ILI9341)

The display connects over 4-wire hardware SPI.

| Display Pin | Label | Wire to ESP32-S3 | Notes |
|-------------|-------|------------------|-------|
| VCC | Power | 3V3 | |
| GND | Ground | GND | |
| SCL / SCK | SPI Clock | GPIO 12 | |
| SDA / MOSI | SPI Data | GPIO 11 | |
| RES / RST | Reset | GPIO 14 | |
| DC / RS | Data/Command | GPIO 13 | |
| CS | Chip Select | GPIO 10 | |
| BLK / BL | Backlight | GPIO 15 | Firmware controls brightness via PWM |

> 🟡 Some ST7789 boards label the pins differently. The signal names above are the logical names — match by function, not label text.

---

## Module 2 — INMP441 I2S Digital Microphone

The INMP441 outputs audio over the I2S protocol (digital — no ADC noise).

| INMP441 Pin | Wire to ESP32-S3 | Notes |
|-------------|-----------------|-------|
| VDD | 3V3 | |
| GND | GND | |
| WS | GPIO 4 | Word Select / LRCLK |
| SCK / BCLK | GPIO 5 | Bit Clock |
| SD | GPIO 6 | Serial Data output from mic |
| **L/R** | **GND** | Selects LEFT channel — must be connected |

> ✅ The L/R pin **must** be tied to GND or 3V3. Leaving it floating causes random/no output. GND = left channel, 3V3 = right channel.

---

## Module 3 — MAX98357A I2S DAC + Amplifier

Drives the 8Ω speaker directly with no additional components.

| MAX98357A Pin | Wire to ESP32-S3 | Notes |
|---------------|-----------------|-------|
| VIN | 3V3 or 5V | 5V from VBUS gives louder output; 3.3V is fine for desk use |
| GND | GND | |
| BCLK | GPIO 7 | Bit Clock |
| LRC / WS | GPIO 8 | Word Select / LRCLK |
| DIN | GPIO 9 | Serial Data into DAC |
| GAIN | Leave floating | Floating = 9 dB gain (default, recommended) |
| SD_MODE | Leave floating or → 3V3 | Floating = always on |
| **OUT+** | Speaker + terminal | |
| **OUT−** | Speaker − terminal | |

> 🔊 Connect the 8Ω 1W speaker's two wires directly to OUT+ and OUT−. Polarity matters for phase — swap if audio sounds thin.

---

## Module 4 — MPU-6050 6-Axis IMU

Connects over I2C. Used for shake and drop detection in Mode 1.

| MPU-6050 Pin | Wire to ESP32-S3 | Notes |
|--------------|-----------------|-------|
| VCC | 3V3 | |
| GND | GND | |
| SDA | GPIO 1 | I2C Data |
| SCL | GPIO 2 | I2C Clock |
| **AD0** | **GND** | Sets I2C address to 0x68 (tie to 3V3 for 0x69) |
| INT | Not connected | Optional — can wire to any GPIO for hardware interrupt |
| XDA / XCL | Not connected | Auxiliary I2C for external magnetometer — unused |

---

## Module 5 — FSR-402 Force Sensitive Resistor

The FSR is a variable resistor. It must be wired as a **voltage divider** with a 10kΩ pull-down resistor so the ESP32 ADC can read pressure as a voltage level.

### Wiring Diagram

```
3.3V ──────┬──────── [FSR-402 Pin 1]
           │
           │          [FSR-402 Pin 2] ──┬──── GPIO 3 (ADC read)
           │                             │
           │                            [10kΩ resistor]
           │                             │
          GND ────────────────────────────┘
```

### Step-by-step

1. Connect **one FSR leg** to 3.3V
2. Connect the **other FSR leg** to GPIO 3
3. Connect a **10kΩ resistor** between GPIO 3 and GND

When no pressure is applied: GPIO 3 reads ~0 (FSR resistance → ∞)
When pressed gently: GPIO 3 reads ~500–2000 (ADC counts out of 4095)
When pressed hard: GPIO 3 reads ~2000–4095

These thresholds are set in `Config.h`:
```cpp
#define FSR_GENTLE_MIN  300
#define FSR_GENTLE_MAX 2000
#define FSR_HARSH_MIN  2001
```

---

## Module 6 — Mode Select Button

**No soldering needed.** The ESP32-S3 DevKitC-1 has a BOOT button on GPIO 0 already built onto the board. This is your mode switch.

- Press once → switch to next mode
- 250ms software debounce prevents accidental triggers

If you want an **external button** instead:
1. Connect one leg of a tactile switch to **GPIO 0**
2. Connect the other leg to **GND**
3. The internal pull-up resistor is enabled in firmware — no external resistor needed

---

## Complete Wire Count Summary

| Module | Wires needed |
|--------|-------------|
| TFT Display | 8 wires |
| INMP441 Mic | 5 wires (including L/R → GND) |
| MAX98357A DAC | 5 wires + 2 speaker wires |
| MPU-6050 | 4 wires |
| FSR-402 | 3 wires + 1 resistor |
| Mode Button | Built-in (0 extra wires) |
| **Total** | **~27 wires** |

---

## Power Budget Check

All modules powered from ESP32-S3 3V3 pin (max 500mA from USB):

| Module | Current Draw |
|--------|-------------|
| ST7789 TFT (with backlight) | ~80 mA |
| INMP441 Mic | ~1.4 mA |
| MAX98357A (idle) | ~2 mA |
| MAX98357A (peak audio) | ~300 mA |
| MPU-6050 | ~3.9 mA |
| FSR-402 circuit | ~0.3 mA |
| ESP32-S3 core | ~150–250 mA |
| **Total peak** | **~640 mA** |

> ⚠️ Peak draw (audio playing) approaches USB limit. Use a USB port rated ≥1A, or power the MAX98357A from 5V VBUS instead of 3.3V. The VBUS pin on the DevKit is connected directly to USB 5V and can supply more current.

---

## Breadboard vs Perfboard

For **testing:** Use a full-size 830-point breadboard. All modules have 2.54mm pitch pins compatible with standard breadboard jumper wires.

For **permanent build:** Transfer to a perfboard (veroboard) and use solid core hookup wire. This eliminates the most common failure mode — loose jumper connections.

Consider ordering a small custom PCB from JLCPCB or PCBWay once the design is finalized — 5 boards cost under ₹500.

---

## Common Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| INMP441 L/R left floating | Microphone outputs garbage or nothing | Tie L/R to GND |
| FSR without pull-down resistor | ADC always reads 4095 | Add 10kΩ from GPIO3 to GND |
| MPU-6050 AD0 floating | I2C scanner finds nothing or wrong address | Tie AD0 to GND |
| TFT BL pin not connected | Display shows but is very dim or off | Wire BL to GPIO 15 |
| SPI device sharing | Display flickers | Each SPI device needs its own CS pin |
