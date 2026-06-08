# ✈️ AeroSniffer
### A Multi-Boot ESP32-S3 Desk Gadget

> **Three devices in one.** Long-press the touch sensor to switch between a living desktop companion, a wireless security monitor, and a real-time flight radar — all running on a single XIAO ESP32S3.

---

## 🧠 What Is This?

AeroSniffer treats the ESP32-S3 like a mini operating system with three completely separate identities:

| Mode | Name | What It Does |
|------|------|--------------|
| 🐾 **Mode 1** | Cyber-Pet | PC-driven interactive desk companion with smooth, high-FPS vector face expressions that react to your computer's activity (typing, CPU load, apps) |
| 🛡️ **Mode 2** | Network Auditor | 802.11 packet sniffer with animated radar sweep display + companion web app ([aero-sniffer.vercel.app](https://aero-sniffer.vercel.app/)) for full security control |
| ✈️ **Mode 3** | Flight Radar | Live ADS-B flight tracker pulling from OpenSky Network API with callsign, altitude, speed, and compass heading |

**Switch modes instantly** with a 1.5-second long-press on the capacitive touch sensor. No reboot needed — FreeRTOS handles clean teardown and re-init of all hardware between modes.

---

## 🛒 Hardware

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

---

## 🔌 Wiring At A Glance

> Full wiring guide: ![Wiring](assets/WIRING.png)

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
- **Wi-Fi Credentials** (for Mode 3)
- **GPS Bounding Box** (for Mode 3 Flight Radar)
- **Theme Colors** (for the UI)

Settings are permanently saved to the ESP32's Non-Volatile Flash memory.

---

## 🧩 Hardware Architecture

![Hardware Architecture](assets/ESP32%20Hardware%20Control-Workflow.png)

---

## 🐾 Mode 1: Cyber-Pet PC Agent

AeroSniffer now features a fully autonomous PC Agent that connects to the ESP32 over USB Serial. It monitors your active windows, CPU usage, and keyboard activity, beaming real-time emotional states directly to the robot's smooth, high-FPS geometric vector face!

**How to run it:**
1. Connect your AeroSniffer via USB and switch to **Mode 1**.
2. Install the dependencies (one-time setup):
   - Open a terminal in the `pc-agent` directory and run `pip install -r requirements.txt`
3. To start the agent seamlessly, simply double-click the **`Start_DeskBuddy.bat`** file in the root folder. It will launch the agent silently in the background!
4. *(Optional)* Add a shortcut to `Start_DeskBuddy.bat` in your Windows `shell:startup` folder to have your pet wake up automatically when you boot your PC.

The robot will now react when you type, panic when your CPU spikes, and fall asleep when you step away! You can easily map your own apps to custom faces by editing `pc-agent/pc_agent.py`.

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

![Web App Architecture](assets/Webapp-workflow.png)

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

Built with ❤️ on XIAO ESP32S3 | India
