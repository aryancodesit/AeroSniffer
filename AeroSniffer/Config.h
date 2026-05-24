// ================================================================
//  Config.h  —  Global Hardware Abstraction Layer
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  HARDWARE VARIANT: DeskBuddy 2.0 Kit
//    MCU:     Seeed Studio XIAO ESP32S3
//    Display: 1.3" ST7789 240×240 Square IPS (SPI)
//    Touch:   Red capacitive touch switch module (digital)
//    Case:    Pre-made 3D-printed enclosure
//
//  Edit only this file to remap any hardware pin.
// ================================================================
#pragma once

// ── Hardware Variant Toggle ──────────────────────────────────────
//    Uncomment ONE line to select your build target.
//    This controls pin mapping, display driver, and feature flags.
// #define HW_DEVKITC         // Original: ESP32-S3 DevKitC-1 + ILI9341 240×320
#define HW_DESKBUDDY_2       // DeskBuddy 2.0: XIAO ESP32S3 + ST7789 240×240

// ================================================================
//  DESKBUDDY 2.0 HARDWARE PROFILE
// ================================================================
#ifdef HW_DESKBUDDY_2

// ── TFT Display — 4-Wire SPI (ST7789 240×240) ──────────────────
//    XIAO ESP32S3 hardware SPI pins + 2 control GPIOs
#define TFT_MOSI         9      // D10 → GPIO 9  (XIAO hardware SPI MOSI)
#define TFT_SCLK         7      // D8  → GPIO 7  (XIAO hardware SPI SCK)
#define TFT_CS           4      // D3  → GPIO 4
#define TFT_DC           3      // D2  → GPIO 3
#define TFT_RST          8      // D9  → GPIO 8
#define TFT_BL          -1      // Backlight always-on (wired to VCC on carrier)

// ── Display Resolution ───────────────────────────────────────────
#define TFT_W           240
#define TFT_H           240     // Square display

// ── Capacitive Touch Switch (DeskBuddy 2.0 Red Touch Module) ────
//    Digital input: LOW when touched, HIGH when released
#define TOUCH_PIN         1      // D0 → GPIO 1
#define TOUCH_DEBOUNCE_MS 200    // Software debounce (ms)

// ── Mode-Select — Long-Press on Touch Sensor ────────────────────
//    Short tap  (<1.5s) = pet interaction / context action
//    Long press (≥1.5s) = cycle to next mode
#define MODE_BTN_PIN     -1      // No separate button; touch handles it
#define LONG_PRESS_MS   1500     // Hold duration to trigger mode switch
#define DEBOUNCE_MS      250     // General debounce

// ── Feature Flags — What's wired? ───────────────────────────────
//    Set to 1 when you solder the optional module into the case.
#define HAS_MIC           0      // INMP441 I2S microphone (not in kit)
#define HAS_DAC           0      // MAX98357A I2S DAC + speaker (not in kit)
#define HAS_IMU           0      // MPU-6050 6-axis IMU (not in kit)
#define HAS_FSR           0      // FSR-402 force sensor (replaced by touch)
#define HAS_TOUCH         1      // Capacitive touch switch (in kit)

// ── I2S Microphone — INMP441 (Optional — solder later) ──────────
#if HAS_MIC
  #define I2S_MIC_WS      2      // Assign to a free GPIO if wired
  #define I2S_MIC_SCK    44      // D7 → GPIO 44
  #define I2S_MIC_SD     43      // D6 → GPIO 43
#endif

// ── I2S DAC / Amplifier — MAX98357A (Optional) ─────────────────
#if HAS_DAC
  #define I2S_DAC_BCLK    2      // Assign to free GPIOs if wired
  #define I2S_DAC_LRC    44
  #define I2S_DAC_DIN    43
#endif

// ── MPU-6050  6-Axis IMU — I2C (Optional) ──────────────────────
#define I2C_SDA           5      // D4 → GPIO 5  (XIAO default I2C SDA)
#define I2C_SCL           6      // D5 → GPIO 6  (XIAO default I2C SCL)
#define MPU6050_I2C_ADDR 0x68    // AD0 pin → GND; use 0x69 if AD0 → VCC

// ── FSR-402 Force / Pressure Sensor — NOT PRESENT ──────────────
//    Replaced by capacitive touch on DeskBuddy 2.0
#if HAS_FSR
  #define FSR_PIN          3
  #define FSR_IDLE_MAX    80
  #define FSR_GENTLE_MIN 300
  #define FSR_GENTLE_MAX 2000
  #define FSR_HARSH_MIN  2001
#endif

// ================================================================
//  ORIGINAL DEVKITC HARDWARE PROFILE (kept for reference)
// ================================================================
#elif defined(HW_DEVKITC)

// ── TFT Display — 4-Wire SPI (ILI9341 240×320) ────────────────
#define TFT_MOSI        11
#define TFT_SCLK        12
#define TFT_CS          10
#define TFT_DC          13
#define TFT_RST         14
#define TFT_BL          15

// ── Display Resolution ──────────────────────────────────────────
#define TFT_W           240
#define TFT_H           320

// ── I2S Microphone — INMP441 ────────────────────────────────────
#define I2S_MIC_WS       4
#define I2S_MIC_SCK      5
#define I2S_MIC_SD       6

// ── I2S DAC / Amplifier — MAX98357A ─────────────────────────────
#define I2S_DAC_BCLK     7
#define I2S_DAC_LRC      8
#define I2S_DAC_DIN      9

// ── MPU-6050  6-Axis IMU — I2C ──────────────────────────────────
#define I2C_SDA          1
#define I2C_SCL          2
#define MPU6050_I2C_ADDR 0x68

// ── FSR-402 Force / Pressure Sensor — ADC ───────────────────────
#define FSR_PIN           3
#define FSR_IDLE_MAX     80
#define FSR_GENTLE_MIN  300
#define FSR_GENTLE_MAX 2000
#define FSR_HARSH_MIN  2001

// ── Mode-Select Button ──────────────────────────────────────────
#define MODE_BTN_PIN      0      // Active-LOW, internal pull-up (BOOT btn)
#define DEBOUNCE_MS     250

// ── Feature flags — all hardware present on DevKitC build ───────
#define HAS_MIC           1
#define HAS_DAC           1
#define HAS_IMU           1
#define HAS_FSR           1
#define HAS_TOUCH         0
#define TOUCH_PIN        -1
#define LONG_PRESS_MS   1500

#else
  #error "No hardware variant defined! Uncomment HW_DESKBUDDY_2 or HW_DEVKITC in Config.h"
#endif

// ================================================================
//  SHARED CONSTANTS (same for all hardware variants)
// ================================================================

// ── System constants ─────────────────────────────────────────────
#define TOTAL_MODES       3      // Pet | Security | Aviation

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

// ── Security Mode Web UI (DeskBuddy 2.0) ────────────────────────
#define SEC_AP_SSID    "AeroSniffer-SEC"
#define SEC_AP_PASS    "sniff1234"
#define SEC_WEB_PORT   80

// ── Aviation Mode Timings (Mode 3) ──────────────────────────────
#define FETCH_INTERVAL_MS 15000   // OpenSky poll rate (respect rate limit)
#define CARD_SCROLL_MS     4000   // Auto-scroll between flight cards
#define MAX_FLIGHTS         10    // Max flights stored in RAM
