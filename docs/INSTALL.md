# 📦 Installation Guide — AeroSniffer

Complete software setup from zero to flashed firmware. Follow every step in order.

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

1. Go to **Tools → Board → esp32 → ESP32S3 Dev Module**
2. Configure these settings under **Tools**:

   | Setting | Value |
   |---------|-------|
   | Board | ESP32S3 Dev Module |
   | Flash Size | 16MB (128Mb) |
   | Flash Mode | QIO 80MHz |
   | PSRAM | OPI PSRAM |
   | Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
   | USB Mode | Hardware CDC and JTAG |
   | Upload Speed | 921600 |

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

| Library | Author | Version |
|---------|--------|---------|
| **TFT_eSPI** | Bodmer | Latest |
| **ArduinoFFT** | kosme | Latest |
| **ArduinoJson** | Benoit Blanchon | 7.x |
| **NimBLE-Arduino** | h2zero | Latest |
| **AnimatedGIF** | Larry Bank | Latest |

> ⚠️ Do **not** install the older `arduinoFFT` (lowercase). Search for `ArduinoFFT` by `kosme`.

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

> ✅ **ILI9341 (240×320 rectangular):** Default config — no changes needed after copy.
> 🟡 **ST7789 (240×240 square):** In the copied file, comment out `#define ILI9341_DRIVER` and uncomment `#define ST7789_DRIVER`. Also change `TFT_HEIGHT` to 240 in both `TFT_eSPI_UserSetup.h` and `Config.h`.

---

## Step 6 — Edit Your Config

Open `AeroSniffer/Config.h` and update these values:

```cpp
// Your home WiFi (used by Mode 2 scan base and Mode 3 API)
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

---

## Step 7 — Upload SPIFFS Data (airlines + aircraft database)

Mode 3 uses a local SQLite-style lookup database for airline names.

1. Install the **LittleFS Upload Plugin** for Arduino IDE:
   - Download from: https://github.com/earlephilhower/arduino-littlefs-upload/releases
   - Place the `.vsix` file in your Arduino IDE plugins folder
   - Restart Arduino IDE

2. With your project open, go to **Tools → LittleFS Data Upload**

3. Wait for the upload to complete (you'll see "SPIFFS Upload complete" in the console)

> 📝 The `data/` folder contains `airlines.db` and `aircraft.db`. These are sourced from the rzeldent esp32-flightradar24-ttgo project and contain ICAO codes for ~1000 airlines and aircraft types.

---

## Step 8 — Compile and Flash

1. Open: **File → Open → `AeroSniffer/AeroSniffer.ino`**
2. Connect ESP32-S3 via USB-C
3. Select the correct **Port** under Tools → Port
4. Click the **Upload** button (→ arrow)
5. Watch the console — you should see:

```
Connecting...
Chip is ESP32-S3 (revision 0)
...
Writing at 0x00010000... (100 %)
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

6. The display should show the **AeroSniffer boot splash**, then enter **Mode 1 (Pet)**

---

## Step 9 — Verify Each Mode

| Test | What to check |
|------|--------------|
| Boot splash | "AeroSniffer" text appears for ~3 seconds |
| Mode 1 (Pet) | Animated eyes appear. Press on FSR → happy expression |
| Mode switch | Press BOOT button → "SECURITY" transition screen → Mode 2 loads |
| Mode 2 (Security) | Header shows CH:01, packet counters increment |
| Mode switch again | Press BOOT → "AVIATION" → Mode 3 loads |
| Mode 3 (Aviation) | Shows "Connecting to WiFi..." then flight cards or "No aircraft" |

---

## Troubleshooting

### Display shows nothing / white screen
- Double-check TFT_eSPI User_Setup.h was correctly replaced
- Verify SPI wiring: MOSI=11, SCLK=12, CS=10, DC=13, RST=14
- Try setting `SPI_FREQUENCY` to `27000000` (slower) in User_Setup.h

### "Guru Meditation Error: Core 1 panic"
- Stack overflow in UI task — increase stack size in `.ino`:
  ```cpp
  xTaskCreatePinnedToCore(task_core1, "UI_Engine", 16384, ...
  ```

### Mode 3 shows "WiFi FAILED"
- Check SSID and password in Config.h — they are case-sensitive
- Ensure your router is 2.4GHz (ESP32-S3 does not support 5GHz)

### Mode 2 shows 0 packets
- Try near a WiFi router and wait 5–10 seconds for channel hop to find traffic
- Check that `esp_wifi_set_promiscuous(true)` isn't being blocked (requires NULL WiFi mode)

### Serial output for debugging
Open **Tools → Serial Monitor** at **115200 baud** after flashing.

---

## Updating Firmware

```bash
git pull origin main
# Re-open .ino in Arduino IDE and re-upload
```

No other steps needed — Config.h persists your personal settings since it is not tracked by git (see `.gitignore`).

> 📌 Tip: Copy your Config.h to a safe location before pulling updates, then merge changes manually.
