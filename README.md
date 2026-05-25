# ✈️ AeroSniffer
### A Multi-Boot ESP32-S3 Desk Gadget

> **Three devices in one.** Long-press the touch sensor to switch between a living desktop companion, a wireless security monitor, and a real-time flight radar — all running on a single XIAO ESP32S3 inside a DeskBuddy 2.0 enclosure.

---

## 🧠 What Is This?

AeroSniffer treats the ESP32-S3 like a mini operating system with three completely separate identities:

| Mode | Name | What It Does |
|------|------|--------------|
| 🐾 **Mode 1** | The Companion | Interactive desk pet with animated face expressions, capacitive touch interaction (head-pats!), and automatic blinking |
| 🛡️ **Mode 2** | Network Auditor | 802.11 packet sniffer with animated radar sweep display + companion web app ([aero-sniffer.vercel.app](https://aero-sniffer.vercel.app/)) for full Marauder control |
| ✈️ **Mode 3** | Flight Radar | Live ADS-B flight tracker pulling from OpenSky Network API with callsign, altitude, speed, and compass heading |

**Switch modes instantly** with a 1.5-second long-press on the capacitive touch sensor. No reboot needed — FreeRTOS handles clean teardown and re-init of all hardware between modes.

---

## 🛒 Hardware: DeskBuddy 2.0 Kit (₹2,299)

> **One purchase, everything included.** No extra components needed.

| # | Component | Included |
|---|-----------|----------|
| 1 | Seeed Studio XIAO ESP32S3 | ✅ |
| 2 | 1.3" ST7789 240×240 IPS Display | ✅ |
| 3 | Red Capacitive Touch Module | ✅ |
| 4 | 3.7V LiPo Battery | ✅ |
| 5 | On/Off Switch | ✅ |
| 6 | 3D Printed Enclosure | ✅ |
| 7 | USB-C Cable | ✅ |

**Buy:** [ESC Labs — DeskBuddy 2.0 Kit](https://www.esclabs.in/product/deskbuddy-2-0-kit/)

See [docs/HARDWARE.md](docs/HARDWARE.md) for full specs, pin allocation, and purchase links.

---

## ⚡ Quick Start

```bash
# 1. Clone this repo
git clone https://github.com/aryancodesit/AeroSniffer.git
cd AeroSniffer

# 2. Install Arduino libraries (run once)
bash tools/install_libraries.sh

# 3. Configure TFT_eSPI for your display
#    Copy the contents of AeroSniffer/TFT_eSPI_UserSetup.h
#    into: ~/Arduino/libraries/TFT_eSPI/User_Setup.h

# 4. Open in Arduino IDE
#    File → Open → AeroSniffer/AeroSniffer.ino
#    Board: XIAO_ESP32S3
#    USB CDC On Boot: Enabled
#    Flash → Upload

# 5. Configure via Web App
#    Go to https://aero-sniffer.vercel.app/
#    Plug in your device, connect via USB, and use the dashboard
#    to save your Wi-Fi, GPS coordinates, and screensaver colors!
```

See [docs/INSTALL.md](docs/INSTALL.md) for the full step-by-step with screenshots.

---

## 🔌 Wiring At A Glance

> Full wiring guide: [docs/WIRING.md](docs/WIRING.md)

```
XIAO ESP32S3
│
├── SPI  → ST7789 1.3" Display (240×240)
│          MOSI=D10(9)  SCLK=D8(7)  CS=D3(4)  DC=D2(3)  RST=D9(8)
│
├── DIG  → Capacitive Touch Module
│          D0(GPIO1) — active LOW, pull-up enabled
│          Short tap  = pet interaction
│          Long press = switch mode (1.5s)
│
├── USB  → Companion App (Mode 2 control)
│          Native USB-C Serial for Marauder commands
│          Access the dashboard: https://aero-sniffer.vercel.app/
│
└── WiFi → Mode 3 Flight Radar (STA mode)
           Connects to home WiFi for OpenSky API
```

---

## 📁 Repository Structure

```
AeroSniffer/
│
├── AeroSniffer/                   ← Arduino sketch (open this in IDE)
│   ├── AeroSniffer.ino            ← Main file — orchestration + FreeRTOS tasks
│   ├── Config.h                   ← ⭐ EDIT THIS — pins, WiFi, bounding box
│   ├── Mode1_Pet.h                ← Desk companion — touch, animations
│   ├── Mode2_Security.h           ← WiFi sniffer — radar display + web UI
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

## ⚙️ Configuration is Now Browser-Based!

No need to hardcode passwords in C++! Once you flash the firmware, simply plug the device into your computer via USB-C and open the Companion Web App:
**[https://aero-sniffer.vercel.app/](https://aero-sniffer.vercel.app/)**

Using the **Web Serial API**, you can instantly configure:
- **Wi-Fi Credentials** (for Mode 1 and Mode 3)
- **GPS Bounding Box** (for Mode 3 Flight Radar)
- **Screensaver Colors** (for Mode 1 Clock/Weather UI)

Settings are permanently saved to the ESP32's Non-Volatile Flash memory.

---

## 🧩 Hardware Architecture

![Hardware Architecture](docs/ESP32%20Hardware%20Control-Workflow.png)

---

## 🛡️ Mode 2: Security Monitor + Companion App

Mode 2 uses a split architecture optimized for the tiny 1.3" display:

**On the device (240×240 screen):**
- Animated radar sweep visualization
- Live PKT/s bar graph
- Packet type counters (beacons, probes, deauths)
- Deauth spike alert indicator

**On your phone/laptop (Companion Web App):**
- Connect via **Web Serial API** over USB-C
- Full Marauder-style scan controls & AP telemetry
- Device configuration (Wi-Fi, Bounding Box, Colors)
- Live Event Log for Deauth attacks

![Web App Architecture](docs/Webapp-workflow.png)

---

## 📚 Inspired By

| Project | Author | What We Borrowed |
|---------|--------|-----------------|
| [Dasai Mochi](https://github.com/maraulsav/Dasai-Mochi) + [TFT Clone](https://github.com/huykhoong/esp32_dasai_mochi_clone_and_how_to) | maraulsav / huykhoong | Pet animation pipeline, expression state machine |
| [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder) | justcallmekoko | Promiscuous WiFi engine, packet classification logic |
| [esp32-flightradar24-ttgo](https://github.com/rzeldent/esp32-flightradar24-ttgo) | rzeldent | Flight API parsing, airline DB structure, card layout |
| [DeskBuddy 2.0](https://www.esclabs.in/product/deskbuddy-2-0-kit/) | ESC Labs | Hardware kit, enclosure design, XIAO form factor |

All firmware in this repository is original code written for ESP32-S3 with FreeRTOS dual-core architecture.

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

Built with ❤️ on XIAO ESP32S3 | Bhubaneswar, Odisha, India
