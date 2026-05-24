# 📦 Installation Guide — AeroSniffer (DeskBuddy 2.0 Kit)

Complete software setup from zero to flashed firmware on the XIAO ESP32S3.

---

## Step 1 — Install Arduino IDE 2.x

Download from: https://www.arduino.cc/en/software

Install the **2.x version** (not legacy 1.8). The IDE 2 has better serial monitoring and library management.

---

## Step 2 — Add ESP32 Board Support

1. Open Arduino IDE
2. Go to **File → Preferences**
3. In the **"Additional boards manager URLs"** field, paste:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Click **OK**
5. Go to **Tools → Board → Boards Manager**
6. Search for `esp32`
7. Install **"esp32 by Espressif Systems"** — version **3.0.x or later**

---

## Step 3 — Select Your Board

1. Go to **Tools → Board → esp32 → XIAO_ESP32S3**
2. Configure these settings under **Tools**:

   | Setting | Value |
   |---------|-------|
   | Board | XIAO_ESP32S3 |
   | Flash Size | 8MB (64Mb) |
   | PSRAM | OPI PSRAM |
   | USB CDC On Boot | **Enabled** |
   | Upload Speed | 921600 |

> ⚠️ **USB CDC On Boot: Enabled** is critical! The XIAO ESP32S3 uses native USB — without this, Serial Monitor won't work.

---

## Step 4 — Install Required Libraries

### Option A — Automatic (recommended)

**Linux / macOS:**
```bash
bash tools/install_libraries.sh
```

**Windows:**
```bat
tools\install_libraries.bat
```

### Option B — Manual via Library Manager

Open **Sketch → Include Library → Manage Libraries**, then search and install each:

| Library | Author | Version | Used By |
|---------|--------|---------|---------|
| **TFT_eSPI** | Bodmer | Latest | Display rendering (all modes) |
| **ArduinoJson** | Benoit Blanchon | 7.x | Flight API parsing (Mode 3) |

> 💡 That's only **2 libraries** for the DeskBuddy 2.0 build! The original DevKitC build needed ArduinoFFT and NimBLE too, but those are for the mic and BLE features which aren't wired on this kit.

---

## Step 5 — Configure TFT_eSPI Library

This is the most important step. TFT_eSPI needs to know your exact pin wiring.

1. Find your Arduino libraries folder:
   - **Linux/Mac:** `~/Arduino/libraries/TFT_eSPI/`
   - **Windows:** `C:\Users\<YourName>\Documents\Arduino\libraries\TFT_eSPI\`

2. Open `User_Setup.h` in that folder with any text editor

3. **Replace the entire contents** with the contents of:
   ```
   AeroSniffer/TFT_eSPI_UserSetup.h
   ```

4. Save and close

5. In the same folder, open `User_Setup_Select.h` and confirm only this line is uncommented:
   ```cpp
   #include <User_Setup.h>
   ```

> ✅ The file is pre-configured for the DeskBuddy 2.0 kit: **ST7789 driver**, **240×240**, XIAO SPI pins.

---

## Step 6 — Edit Your Config

Open `AeroSniffer/Config.h` and update these values:

```cpp
// Your home WiFi (used by Mode 2 AP fallback and Mode 3 API)
#define WIFI_SSID        "YOUR_WIFI_NAME"
#define WIFI_PASSWORD    "YOUR_WIFI_PASSWORD"

// Bounding box for flight radar (Mode 3)
// Get yours at: https://boundingbox.klokantech.com
// Select CSV RAW format — order is: minLon, minLat, maxLon, maxLat
// Default below covers ~60km radius over Bhubaneswar, Odisha
#define SKY_LAMIN   19.8f
#define SKY_LOMIN   85.0f
#define SKY_LAMAX   21.0f
#define SKY_LOMAX   86.8f
```

The hardware variant is already set to `HW_DESKBUDDY_2`. No pin changes needed.

---

## Step 7 — Upload SPIFFS Data (airlines + aircraft database)

Mode 3 uses a local lookup database for airline names.

1. Install the **LittleFS Upload Plugin** for Arduino IDE:
   - Download from: https://github.com/earlephilhower/arduino-littlefs-upload/releases
   - Place the `.vsix` file in your Arduino IDE plugins folder
   - Restart Arduino IDE

2. With your project open, go to **Tools → LittleFS Data Upload**

3. Wait for the upload to complete (you'll see "SPIFFS Upload complete" in the console)

> 📝 The `data/` folder contains `airlines.db` and `aircraft.db` for ICAO code lookups.

---

## Step 8 — Compile and Flash

1. Open: **File → Open → `AeroSniffer/AeroSniffer.ino`**
2. Connect the XIAO ESP32S3 via USB-C
3. Select the correct **Port** under Tools → Port
4. Click the **Upload** button (→ arrow)
5. Watch the console — you should see:

```
Connecting...
Chip is ESP32-S3
...
Writing at 0x00010000... (100 %)
Hash of data verified.
```

6. The display should show the **AeroSniffer boot splash**, then enter **Mode 1 (Pet)**

> 💡 If upload fails, put the XIAO into download mode: **Hold BOOT → Press RESET → Release BOOT**, then try again.

---

## Step 9 — Verify Each Mode

| Test | What to check |
|------|--------------|
| Boot splash | "AeroSniffer" text appears for ~3 seconds |
| Mode 1 (Pet) | Animated eyes appear. Tap the touch sensor → happy expression |
| Mode switch | Long-press touch sensor (1.5s) → "SECURITY" transition → Mode 2 loads |
| Mode 2 (Security) | Animated radar sweep, packet counters visible. Connect phone to `AeroSniffer-SEC` WiFi and open `http://192.168.4.1` |
| Mode switch again | Long-press → "AVIATION" → Mode 3 loads |
| Mode 3 (Aviation) | Shows "Connecting to WiFi..." then flight cards or "No aircraft" |

---

## Troubleshooting

### Display shows nothing / white screen
- Double-check TFT_eSPI `User_Setup.h` was correctly replaced
- Verify SPI wiring: MOSI=D10(9), SCLK=D8(7), CS=D3(4), DC=D2(3), RST=D9(8)
- Try setting `SPI_FREQUENCY` to `27000000` (slower) in User_Setup.h

### Cannot upload — "No serial port detected"
- The XIAO ESP32S3 uses **native USB** — no CH340/CP2102 driver needed
- Try download mode: Hold BOOT → Press RESET → Release BOOT
- Check USB cable supports data (not charge-only)

### "Guru Meditation Error: Core 1 panic"
- Stack overflow in UI task — increase stack size in `.ino`:
  ```cpp
  xTaskCreatePinnedToCore(task_core1, "UI_Engine", 16384, ...
  ```

### Mode 3 shows "WiFi FAILED"
- Check SSID and password in Config.h — they are case-sensitive
- Ensure your router is 2.4GHz (ESP32-S3 does not support 5GHz)

### Touch sensor not responding
- Verify D0 (GPIO 1) is wired to the SIG/OUT pin of the touch module
- Check that VCC and GND are connected
- The touch module outputs LOW when touched — firmware uses INPUT_PULLUP

### Serial output for debugging
Open **Tools → Serial Monitor** at **115200 baud** after flashing.
Ensure "USB CDC On Boot" is set to **Enabled** in board settings.

---

## Updating Firmware

```bash
git pull origin main
# Re-open .ino in Arduino IDE and re-upload
```

No other steps needed — Config.h persists your personal settings since it is not tracked by git (see `.gitignore`).

> 📌 Tip: Copy your Config.h to a safe location before pulling updates, then merge changes manually.
