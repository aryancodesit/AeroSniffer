# SPIFFS Data Files — AeroSniffer Mode 3

This folder contains the local lookup databases used by Mode 3 (Flight Radar)
to resolve ICAO codes into human-readable airline and aircraft names without
additional API calls.

---

## Files

| File | Size | Contents |
|------|------|---------|
| `airlines.db` | ~40 KB | ICAO airline code → airline name + country |
| `aircraft.db` | ~60 KB | ICAO aircraft type code → manufacturer + model |

These databases are sourced from the
[esp32-flightradar24-ttgo](https://github.com/rzeldent/esp32-flightradar24-ttgo)
project by rzeldent, which compiled them from public ICAO data.

---

## How to Upload to ESP32-S3

These files must be uploaded to the ESP32-S3's flash filesystem (LittleFS)
separately from the sketch firmware.

### Step 1 — Install the Upload Plugin

Download the LittleFS upload plugin for Arduino IDE 2.x:
https://github.com/earlephilhower/arduino-littlefs-upload/releases

Place the `.vsix` file in:
- **Linux/Mac:** `~/.arduinoIDE/plugins/`
- **Windows:** `%USERPROFILE%\.arduinoIDE\plugins\`

Restart Arduino IDE.

### Step 2 — Upload

With `MultiBoot_DeskGadget.ino` open:
1. Go to **Tools → LittleFS Data Upload**
2. Arduino IDE will upload all files in the `data/` folder to the ESP32-S3

### Step 3 — Verify

After the upload, open Serial Monitor (115200 baud). On boot, Mode 3 will
print "airlines.db loaded: OK" if the database was found.

---

## Data Format

The `.db` files are simple newline-delimited text in the format:

```
# airlines.db
ICAO,AirlineName,Country
AAL,American Airlines,United States
DAL,Delta Air Lines,United States
IGO,IndiGo,India
AIC,Air India,India
...
```

You can edit these files in any text editor to add regional carriers.

---

## Updating the Database

To update with the latest ICAO data, pull from the upstream rzeldent repo:
```bash
git clone https://github.com/rzeldent/esp32-flightradar24-ttgo.git /tmp/ttgo
cp /tmp/ttgo/data/airlines.db ./data/airlines.db
cp /tmp/ttgo/data/aircraft.db ./data/aircraft.db
```

Then re-run the LittleFS Data Upload step.
