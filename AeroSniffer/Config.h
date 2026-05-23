// ================================================================
//  Config.h  —  Global Hardware Abstraction Layer
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//  Edit only this file to remap any hardware pin.
// ================================================================
#pragma once

// ── TFT Display — 4-Wire SPI ────────────────────────────────────
//    Works with ST7789 (240x240 square) or ILI9341 (240x320 rect)
#define TFT_MOSI        11      // SPI Data Out → display SDA/MOSI
#define TFT_SCLK        12      // SPI Clock    → display SCL/SCK
#define TFT_CS          10      // Chip Select  → display CS
#define TFT_DC          13      // Data/Command → display DC
#define TFT_RST         14      // Hardware Reset → display RES
#define TFT_BL          15      // Backlight PWM → display BLK

// ── I2S Microphone — INMP441 (I2S Port 0 / RX) ─────────────────
#define I2S_MIC_WS       4      // Word Select (LRCLK)
#define I2S_MIC_SCK      5      // Bit Clock   (BCLK)
#define I2S_MIC_SD       6      // Serial Data (out of mic)
//    INMP441 L/R pin → GND  (selects left channel)

// ── I2S DAC / Amplifier — MAX98357A (I2S Port 1 / TX) ──────────
#define I2S_DAC_BCLK     7      // Bit Clock
#define I2S_DAC_LRC      8      // Word Select (LRC / LRCLK)
#define I2S_DAC_DIN      9      // Serial Data In (to DAC)
//    MAX98357A GAIN → float (9 dB default) | SD_MODE → float or VCC

// ── MPU-6050  6-Axis IMU — I2C ──────────────────────────────────
#define I2C_SDA          1
#define I2C_SCL          2
#define MPU6050_I2C_ADDR 0x68   // AD0 pin → GND; use 0x69 if AD0 → VCC

// ── FSR-402 Force / Pressure Sensor — ADC ───────────────────────
//    Wire: 3.3 V → FSR → GPIO3 & GPIO3 → 10 kΩ → GND  (voltage divider)
#define FSR_PIN           3      // ADC1_CH2  (keep below GPIO 10 for ADC1)
#define FSR_IDLE_MAX     80      // Raw ADC counts  — no touch
#define FSR_GENTLE_MIN  300      // Gentle pat lower bound
#define FSR_GENTLE_MAX 2000      // Gentle pat upper bound
#define FSR_HARSH_MIN  2001      // Harsh press lower bound

// ── Mode-Select Button ───────────────────────────────────────────
//    Wire: GPIO → tactile switch → GND   (internal pull-up enabled)
//    The on-board BOOT button is already on GPIO 0 — use it for free!
#define MODE_BTN_PIN      0      // Active-LOW, internal pull-up
#define DEBOUNCE_MS     250      // Milliseconds between valid presses

// ── System constants ─────────────────────────────────────────────
#define TOTAL_MODES       3      // Pet | Security | Aviation

// ── Display Resolution ───────────────────────────────────────────
#define TFT_W           240
#define TFT_H           240      // ← Change to 320 for ILI9341 rectangular

// ── Wi-Fi Credentials  (Mode 2 scan base / Mode 3 API client) ───
#define WIFI_SSID         "YOUR_WIFI_SSID"
#define WIFI_PASSWORD     "YOUR_WIFI_PASSWORD"

// ── OpenSky Network REST API bounding box ────────────────────────
//    Default: ~60 km radius over Bhubaneswar, Odisha, India
//    Adjust lat/lon bounds to your location (1 degree ≈ 111 km)
#define SKY_LAMIN        19.8f
#define SKY_LOMIN        85.0f
#define SKY_LAMAX        21.0f
#define SKY_LOMAX        86.8f
#define OPENSKY_URL  "http://opensky-network.org/api/states/all" \
                     "?lamin=" STR(SKY_LAMIN) "&lomin=" STR(SKY_LOMIN) \
                     "&lamax=" STR(SKY_LAMAX) "&lomax=" STR(SKY_LOMAX)
// Helper to stringify float macros
#define STR2(x) #x
#define STR(x)  STR2(x)

// ── FFT Parameters (Mode 1 — Music Reactive) ────────────────────
#define FFT_SAMPLES       512     // Must be power of 2
#define FFT_SAMPLE_RATE 22050     // Hz
#define BASS_HZ_MAX       250     // Frequencies ≤ this are "bass"
#define BASS_THRESHOLD  4500.0f   // Magnitude sum to trigger bob

// ── Security Mode Thresholds (Mode 2) ───────────────────────────
#define DEAUTH_SPIKE_COUNT  10    // Deauths in 5 s window = alert
#define MAX_AP_TABLE        64    // Max tracked SSIDs
#define CHANNEL_HOP_MS     180    // Channel dwell time (ms)

// ── Aviation Mode Timings (Mode 3) ──────────────────────────────
#define FETCH_INTERVAL_MS 15000   // OpenSky poll rate (respect rate limit)
#define CARD_SCROLL_MS     4000   // Auto-scroll between flight cards
#define MAX_FLIGHTS         10    // Max flights stored in RAM
