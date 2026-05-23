# ✈️ AeroSniffer
### A Multi-Boot ESP32-S3 Desk Gadget

> **Three devices in one.** Press a single button to switch between a living desktop companion, a wireless security monitor, and a real-time flight radar — all running on a single ESP32-S3.

---

## 🧠 What Is This?

AeroSniffer treats the ESP32-S3 like a mini operating system with three completely separate identities:

| Mode | Name | What It Does |
|------|------|--------------|
| 🐾 **Mode 1** | The Companion | Interactive desk pet with animated face, music-reactive bobbing, FSR haptic sensing, and I2S audio |
| 🛡️ **Mode 2** | Network Auditor | Passive 802.11 packet sniffer — probe requests, beacons, EAPOL handshakes, deauth spike alerts, evil twin detection |
| ✈️ **Mode 3** | Flight Radar | Live ADS-B flight tracker pulling from OpenSky Network API with callsign, altitude, speed, and compass heading |

**Switch modes instantly** with the BOOT button (GPIO 0). No reboot needed — FreeRTOS handles clean teardown and re-init of all hardware between modes.

---

## ⚡ Quick Start (Software Only — No Hardware Yet)

```bash
# 1. Clone this repo
git clone https://github.com/aryancodesit/AeroSniffer.git
cd AeroSniffer

# 2. Install Arduino libraries (run once)
bash tools/install_libraries.sh

# 3. Configure your settings (WiFi + GPS bounding box)
nano AeroSniffer/Config.h

# 4. Configure TFT_eSPI for your display
#    Copy the contents of AeroSniffer/TFT_eSPI_UserSetup.h
#    into: ~/Arduino/libraries/TFT_eSPI/User_Setup.h

# 5. Open in Arduino IDE
#    File → Open → AeroSniffer/AeroSniffer.ino
#    Board: ESP32S3 Dev Module
#    Flash → Upload
```

That's it. See [INSTALL.md](docs/INSTALL.md) for the full step-by-step with screenshots.

---

## 🛒 Hardware Bill of Materials

> **You only need to buy these 7 components.** Total cost estimate: ₹1,500–2,500 depending on supplier.

| # | Component | Model | Qty | Purpose | Buy (India) |
|---|-----------|-------|-----|---------|-------------|
| 1 | **Microcontroller** | ESP32-S3 DevKitC-1 (16MB) | 1 | Main brain | Robu.in / Quartzcomponents |
| 2 | **TFT Display** | ST7789 240×240 SPI *(or ILI9341 240×320)* | 1 | All UI rendering | AliExpress / Robu.in |
| 3 | **Microphone** | INMP441 I2S Digital Mic | 1 | FFT music analysis | Robu.in / Amazon.in |
| 4 | **Amplifier + Speaker** | MAX98357A I2S DAC + 8Ω 1W speaker | 1 | Audio output | Robu.in |
| 5 | **Force Sensor** | FSR-402 Pressure Resistor | 1 | Pet touch sensing | Amazon.in |
| 6 | **IMU** | MPU-6050 6-Axis Gyroscope | 1 | Shake/tilt detection | Anywhere |
| 7 | **Resistor** | 10 kΩ through-hole | 1 | FSR voltage divider | Any electronics shop |

> 🟡 **Note:** The BOOT button (GPIO 0) is already on the ESP32-S3 DevKit — no separate button needed for mode switching.

See [docs/HARDWARE.md](docs/HARDWARE.md) for detailed purchase links and alternatives.

---

## 🔌 Wiring At A Glance

> Full wiring guide with diagrams: [docs/WIRING.md](docs/WIRING.md)

```
ESP32-S3 DevKitC-1
│
├── SPI  → TFT Display (ST7789 / ILI9341)
│          MOSI=11  SCLK=12  CS=10  DC=13  RST=14  BL=15
│
├── I2S0 → INMP441 Microphone (input)
│          WS=4  SCK=5  SD=6   (L/R pin → GND)
│
├── I2S1 → MAX98357A DAC Amplifier (output)
│          BCLK=7  LRC=8  DIN=9
│
├── I2C  → MPU-6050 IMU
│          SDA=1  SCL=2   (AD0 → GND = address 0x68)
│
├── ADC  → FSR-402 Force Sensor
│          3.3V → FSR → GPIO3 ──┬── 10kΩ → GND
│                               └── read here
│
└── GPIO → Mode Button
           GPIO0 (BOOT button, built-in) → Active LOW
```

---

## 📁 Repository Structure

```
AeroSniffer/
│
├── AeroSniffer/                   ← Arduino sketch (open this in IDE)
│   ├── AeroSniffer.ino            ← Main file — orchestration + FreeRTOS tasks
│   ├── Config.h                   ← ⭐ EDIT THIS — all pins, WiFi, bounding box
│   ├── Mode1_Pet.h                ← Desk companion — FFT, FSR, animations, audio
│   ├── Mode2_Security.h           ← WiFi sniffer — promiscuous mode, UI, alerts
│   ├── Mode3_Aviation.h           ← Flight radar — OpenSky API, flight cards
│   └── TFT_eSPI_UserSetup.h       ← Copy this into TFT_eSPI library folder
│
├── data/                          ← SPIFFS data (upload separately)
│   ├── airlines.db                ← Airline ICAO → name lookup (Mode 3)
│   └── aircraft.db                ← Aircraft ICAO → type lookup (Mode 3)
│
├── docs/
│   ├── INSTALL.md                 ← Full installation walkthrough
│   ├── WIRING.md                  ← Pin-by-pin hardware connection guide
│   ├── HARDWARE.md                ← BOM with purchase links
│   └── images/                    ← Wiring diagrams, photos
│
├── tools/
│   ├── install_libraries.sh       ← Auto-install all Arduino libraries (Linux/Mac)
│   └── install_libraries.bat      ← Auto-install all Arduino libraries (Windows)
│
├── .gitignore
└── README.md                      ← You are here
```

---

## ⚙️ The Only File You Need to Edit

Open **`AeroSniffer/Config.h`** and change these 3 sections:

```cpp
// ── Your WiFi credentials ──────────────────────────────
#define WIFI_SSID        "YOUR_WIFI_SSID"
#define WIFI_PASSWORD    "YOUR_WIFI_PASSWORD"

// ── Your location bounding box for flight radar ────────
// Find yours at: boundingbox.klokantech.com
#define SKY_LAMIN   19.8f   // South latitude
#define SKY_LOMIN   85.0f   // West longitude
#define SKY_LAMAX   21.0f   // North latitude
#define SKY_LOMAX   86.8f   // East longitude

// ── Your TFT display resolution ────────────────────────
#define TFT_W  240
#define TFT_H  240   // Change to 320 if using ILI9341 rectangular panel
```

Everything else is pre-configured for the default hardware matrix.

---

## 🧩 Software Architecture

```
                     ┌─────────────────────────────┐
                     │   BOOT Button (GPIO 0 ISR)   │
                     │   cycles mode 0 → 1 → 2 → 0  │
                     └──────────────┬──────────────┘
                                    │ modeChanged flag
                   ┌────────────────▼────────────────┐
                   │        FreeRTOS Task Router       │
                   ├────────────────┬─────────────────┤
                   │    CORE 0      │     CORE 1       │
                   │  (Background)  │   (UI Engine)    │
                   ├────────────────┼─────────────────┤
                   │ FFT / I2S mic  │ TFT rendering    │
                   │ WiFi promiscu. │ FSR + MPU reads  │
                   │ HTTP API calls │ State machine     │
                   └────────────────┴─────────────────┘
```

---

## 📚 Inspired By

This project merges concepts and code patterns from three open-source projects:

| Project | Author | What We Borrowed |
|---------|--------|-----------------|
| [Dasai Mochi](https://github.com/maraulsav/Dasai-Mochi) + [TFT Clone](https://github.com/huykhoong/esp32_dasai_mochi_clone_and_how_to) | maraulsav / huykhoong | Pet animation pipeline, gif2cpp workflow, buzzer patterns |
| [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder) | justcallmekoko | Promiscuous WiFi engine, packet classification logic |
| [esp32-flightradar24-ttgo](https://github.com/rzeldent/esp32-flightradar24-ttgo) | rzeldent | Flight API parsing, airline DB structure, card layout |

All firmware in this repository is original code written for ESP32-S3 with FreeRTOS dual-core architecture. Only the conceptual patterns and data formats were studied from the above projects.

---

## ⚠️ Legal & Ethical Use

Mode 2 (Network Auditor) uses ESP32 promiscuous mode to passively observe 802.11 frames.
**Only monitor networks you own or have explicit written permission to audit.**
Passive capture of publicly-broadcast beacon/probe frames is generally legal.
Active attacks (deauth, beacon spam) are **NOT** implemented in this firmware and are illegal in most jurisdictions.

---

## 📄 License

MIT License — see [LICENSE](LICENSE) for details.

---

## 🙋 Contributing

Issues and PRs welcome. See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) for guidelines.

Built with ❤️ on ESP32-S3 | Bhubaneswar, Odisha, India
