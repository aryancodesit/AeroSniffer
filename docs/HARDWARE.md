# 🛒 Hardware Bill of Materials — AeroSniffer

Full component list with purchase options for India and global alternatives.

---

## Summary

| Total Components | Estimated Cost (India) | Estimated Cost (Global) |
|-----------------|----------------------|------------------------|
| 7 modules + 1 resistor | ₹1,500 – ₹2,500 | USD 18 – 30 |

---

## Component Details

### 1. ESP32-S3 DevKitC-1 (16MB Flash)

| Spec | Value |
|------|-------|
| Chip | ESP32-S3, dual-core Xtensa LX7, 240 MHz |
| Flash | 16 MB |
| PSRAM | 8 MB OPI (on -N16R8 variant) |
| WiFi | 802.11 b/g/n, 2.4 GHz |
| BLE | 5.0 |
| GPIO | 36 programmable pins |

**Buy India:**
- Robu.in — search "ESP32-S3 DevKitC"
- QuartzComponents.com
- Amazon.in — search "ESP32-S3 Dev Board"

**Buy Global:**
- AliExpress — "ESP32-S3-DevKitC-1-N16R8"
- Mouser / Digi-Key (official Espressif variant)

**✅ Accepted alternatives:**
- ESP32-S3-WROOM-1 on custom PCB
- LilyGo T7-S3 (same chip, different form factor)

**❌ NOT compatible:**
- ESP32-C3 (single core, no BLE 5)
- ESP32 OG / ESP32-D0 (different pin mapping, no I2S slave)
- ESP8266 (no BLE, weaker WiFi stack)

---

### 2. TFT Display

**Option A — ST7789 240×240 (Recommended)**

| Spec | Value |
|------|-------|
| Driver IC | ST7789V |
| Resolution | 240 × 240 pixels |
| Interface | 4-wire SPI |
| Voltage | 3.3V |
| Shape | Square |

**Buy:** AliExpress — "1.3 inch ST7789 SPI TFT LCD"

**Option B — ILI9341 240×320 (Rectangular)**

| Spec | Value |
|------|-------|
| Driver IC | ILI9341 |
| Resolution | 240 × 320 pixels |
| Interface | 4-wire SPI |
| Shape | Rectangular (portrait) |

**Buy:** Robu.in — "2.4 inch ILI9341 SPI TFT"

> ⚠️ If using ILI9341, change `TFT_HEIGHT` to 320 in `Config.h` and
> update `TFT_eSPI_UserSetup.h` to use `ILI9341_DRIVER` instead of `ST7789_DRIVER`.

---

### 3. INMP441 I2S Digital Microphone

| Spec | Value |
|------|-------|
| Interface | I2S |
| Frequency Response | 60 Hz – 15 kHz |
| SNR | 61 dB |
| Voltage | 3.3V |

**Buy India:** Robu.in — "INMP441 I2S Microphone"
**Buy Global:** AliExpress — "INMP441 Omnidirectional Microphone"

**Alternative:** ICS-43434 (same I2S protocol, slightly better SNR)

> ❌ Do not use analog microphones (KY-037, MAX4466) — they cannot connect to the I2S peripheral and require a separate ADC which loses audio quality.

---

### 4. MAX98357A I2S DAC + Amplifier

| Spec | Value |
|------|-------|
| Interface | I2S input |
| Output power | 3.2W @ 4Ω, 1.8W @ 8Ω |
| Voltage | 2.5V – 5.5V |
| Gain (default) | 9 dB |

**Buy India:** Robu.in — "MAX98357A I2S Amplifier"
**Buy Global:** Adafruit #3006, or AliExpress

**Speaker:** Any 8Ω 1W speaker. 40mm diameter "mini speaker" from AliExpress works well.

> 💡 If you want louder audio, power MAX98357A from the 5V VBUS pin instead of 3.3V. The audio output pins are still 3.3V-logic compatible.

---

### 5. FSR-402 Force Sensitive Resistor

| Spec | Value |
|------|-------|
| Sensing area | 12.7 mm diameter |
| Force range | 0.1 N – 10 N |
| Resistance at no force | >10 MΩ |
| Resistance at full force | ~200 Ω |

**Buy India:** Amazon.in — "FSR-402 Force Sensitive Resistor"
**Buy Global:** Sparkfun #09375, or Adafruit #166

**Alternative:** FSR-406 (larger sensing area, 38mm circle — better for palm-petting)

**Required companion:** 10 kΩ through-hole resistor (1/4W) — available at any local electronics shop for ₹1.

---

### 6. MPU-6050 6-Axis IMU

| Spec | Value |
|------|-------|
| Sensors | 3-axis accelerometer + 3-axis gyroscope |
| Interface | I2C |
| Voltage | 3.3V (most breakout boards have onboard regulator for 5V too) |
| I2C Address | 0x68 (AD0=GND) or 0x69 (AD0=VCC) |

**Buy India:** Any electronics shop / Amazon.in — "MPU-6050 GY-521"
**Buy Global:** AliExpress — "GY-521 MPU-6050"

> ✅ The GY-521 breakout board is the most common and cheapest form — use this.

---

## Optional / Future Upgrades

| Component | Purpose | When to Add |
|-----------|---------|-------------|
| MicroSD card module | Save PCAP captures from Mode 2 | After v1 is working |
| GPS module (NEO-6M) | Wardriving location tagging in Mode 2 | Advanced feature |
| LiPo battery + TP4056 charger | Portable operation | 3D enclosure phase |
| 3D printed enclosure | Clean desk form factor | Final polish phase |

---

## Where to Buy in India (Ranked by Reliability)

1. **Robu.in** — Best selection, fast shipping, genuine components
2. **Quartzcomponents.com** — Good ESP32 and sensor stock
3. **Amazon.in** — Faster delivery but verify seller ratings
4. **AliExpress** — Cheapest but 2–4 week shipping; good for displays and sensors
5. **Local "Lamington Road equivalent"** — Bhubaneswar: Electronics Market near Old Town / Sahid Nagar for resistors, wires, breadboards

---

## 📦 3D Printed Cube Enclosure (Cheapest & Easiest)

If you are planning to build a case for the AeroSniffer, the easiest form factor is a simple **cube or cuboid**. 

Based on the components listed above, here are the recommended internal dimensions to ensure everything fits comfortably with standard Dupont jumper wires:

### Recommended Dimensions (Internal)
* **Width:** `60 mm`
* **Height:** `60 mm`
* **Depth:** `50 mm`

### Placement Guide
* **Front Face:** 
  * A `26 x 26 mm` square cutout for the 1.3" ST7789 TFT display.
  * A small `2 mm` hole near the bottom for the INMP441 Microphone (it needs air access to hear properly).
* **Top Face:** 
  * Leave a flat surface to adhere the FSR-402 Force Sensor. You can cover the sensor with a piece of felt or silicone for a nicer "petting" feel.
* **Back/Bottom Face:**
  * A `40 mm` circular grid/cutout for the 8Ω speaker to let audio out.
  * A slot for the ESP32-S3's USB-C port to stick out so you can power it and flash updates.
* **Internal:**
  * The ESP32-S3 DevKit is about `55 mm x 26 mm`, so it will fit perfectly diagonally or flat against the side wall.
  * The MPU-6050 and MAX98357A are very small and can be hot-glued to the internal side walls.
