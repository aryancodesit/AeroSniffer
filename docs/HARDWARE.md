# 🛒 Hardware Bill of Materials — AeroSniffer

Full component list for the **DeskBuddy 2.0 Kit** build.

---

## Summary

| Total Components | Kit Cost (India) | Extra Purchases |
|-----------------|-----------------|-----------------|
| 1 kit (all-in-one) | ₹2,299 | ₹0 — nothing else needed |

---

## 🎯 Recommended Build: DeskBuddy 2.0 Kit (₹2,299)

> Purchase from: [ESC Labs — DeskBuddy 2.0 Kit](https://www.esclabs.in/product/deskbuddy-2-0-kit/)

### What's Included

| # | Component | Spec | Purpose |
|---|-----------|------|---------|
| 1 | **Seeed Studio XIAO ESP32S3** | ESP32-S3, 8MB Flash, 8MB PSRAM, WiFi+BLE 5.0 | Main brain |
| 2 | **1.3" IPS TFT Display** | ST7789, 240×240 square, SPI | All UI rendering |
| 3 | **Capacitive Touch Module** | Red touch switch, digital output (active LOW) | Pet interaction + mode switching |
| 4 | **3.7V LiPo Battery** | Matched to enclosure, USB-C charging | Portable operation |
| 5 | **On/Off Switch** | Slide or toggle | Power control |
| 6 | **3D Printed Enclosure** | Custom-designed, professionally finished | Clean desk form factor |
| 7 | **USB-C Cable** | Data + power | Flashing + charging |

### XIAO ESP32S3 Specifications

| Spec | Value |
|------|-------|
| Chip | ESP32-S3, dual-core Xtensa LX7, 240 MHz |
| Flash | 8 MB |
| PSRAM | 8 MB OPI |
| WiFi | 802.11 b/g/n, 2.4 GHz |
| BLE | 5.0 |
| GPIO | 11 usable pins (header) |
| USB | Native USB-C (no external UART chip) |
| Size | 21mm × 17.5mm — thumb-sized! |

### Display Specifications

| Spec | Value |
|------|-------|
| Driver IC | ST7789V |
| Resolution | 240 × 240 pixels |
| Interface | 4-wire SPI |
| Shape | Square |
| Type | IPS (wide viewing angles) |
| Size | 1.3 inches diagonal |

---

## 🔌 Pin Allocation (XIAO ESP32S3)

| XIAO Pin | GPIO # | Allocated To |
|----------|--------|-------------|
| D0 | GPIO 1 | Capacitive touch sensor |
| D1 | GPIO 2 | _Free_ |
| D2 | GPIO 3 | TFT DC (Data/Command) |
| D3 | GPIO 4 | TFT CS (Chip Select) |
| D4 | GPIO 5 | _Free (I2C SDA reserved)_ |
| D5 | GPIO 6 | _Free (I2C SCL reserved)_ |
| D6 | GPIO 43 | _Free (UART TX)_ |
| D7 | GPIO 44 | _Free_ |
| D8 | GPIO 7 | TFT SCLK (SPI Clock) |
| D9 | GPIO 8 | TFT RST (Reset) |
| D10 | GPIO 9 | TFT MOSI (SPI Data) |

> ⚠️ The XIAO ESP32S3 has no GPIO 0 button on the header. Mode switching is handled by a **1.5-second long-press** on the capacitive touch sensor.

---

## 📊 Why the Kit vs Individual Parts

| Criteria | 📦 DeskBuddy 2.0 Kit (₹2,299) | 🔧 Individual Components |
|----------|-------------------------------|--------------------------|
| **Est. Cost** | ₹2,299 flat | ₹1,600–2,100 + multiple shipping fees |
| **Enclosure** | Included — custom 3D-printed case | None — DIY cardboard or ₹400–600 for 3D print |
| **Form Factor** | Ultra-compact, clean, portable | Bulky breadboard + exposed wires |
| **Battery** | Included + matched to case | Source separately + figure out mounting |
| **Convenience** | One package, zero alignment headaches | Multiple deliveries, manual fitment |

**Verdict:** Buy the kit. The enclosure + battery + convenience far outweighs the small savings from sourcing individually.

---

## ❌ Components NOT Needed

The original AeroSniffer design included these modules, but they are **not required** for the DeskBuddy 2.0 build:

| Component | Why Not Needed |
|-----------|---------------|
| ESP32-S3 DevKitC-1 | Replaced by XIAO ESP32S3 (in kit) |
| ILI9341 240×320 display | Replaced by ST7789 240×240 (in kit) |
| INMP441 I2S microphone | Not enough GPIO pins; music-reactive bobbing disabled |
| MAX98357A DAC + speaker | Not enough GPIO pins; sound effects disabled |
| FSR-402 force sensor | Replaced by capacitive touch module (in kit) |
| MPU-6050 IMU | Not included; shake detection disabled |
| 10kΩ resistor | No longer needed (no FSR voltage divider) |

---

## 🏠 Enclosure Notes

The DeskBuddy 2.0 kit comes with a pre-designed 3D-printed enclosure. Key points:

- **Touch sensor placement:** Mount the red capacitive touch module under the roof of the shell for head-pat interaction.
- **Display window:** Pre-cut opening sized for the 1.3" ST7789 module.
- **USB-C access:** Slot for the XIAO's USB-C port for charging and firmware updates.
- **Battery compartment:** Sized for the included LiPo cell.

---

## 🛍️ Where to Buy in India

1. **ESC Labs** — [esclabs.in](https://www.esclabs.in) — The DeskBuddy 2.0 kit
2. **Robu.in** — Sensors, breakout boards, ESP modules
3. **Amazon.in** — Fast delivery, verify seller ratings
4. **AliExpress** — Cheapest for displays and sensors (2–4 week shipping)

---

Built with ❤️ on ESP32-S3 | Bhubaneswar, Odisha, India
