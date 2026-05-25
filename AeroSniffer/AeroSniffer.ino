// ================================================================
//  MultiBoot_DeskGadget.ino  —  Orchestration Layer
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  Architecture:
//    Core 0 → Background processing (FFT / network / API)
//    Core 1 → UI rendering + sensor reads (display thread)
//
//  Mode cycle (touch long-press on DeskBuddy / BOOT button on DevKitC):
//    0 = Companion (Pet)   →   1 = Security Monitor   →   2 = Flight Radar
//
//  Required Libraries (install via Arduino Library Manager):
//    • TFT_eSPI        by Bodmer
//    • ArduinoFFT      by kosme       (only if HAS_MIC)
//    • ArduinoJson     by Benoit Blanchon
//    • (ESP32 Arduino core ships FreeRTOS, WiFi, HTTPClient, I2S, Wire)
//
//  IMPORTANT: Before flashing, configure TFT_eSPI by copying
//  the provided TFT_eSPI_UserSetup.h content into:
//    Arduino/libraries/TFT_eSPI/User_Setup.h
// ================================================================

#include "Config.h"
#include "Mode1_Pet.h"
#include "Mode2_Security.h"
#include "Mode3_Aviation.h"

#include <TFT_eSPI.h>
#include <Wire.h>
#include <Preferences.h>

// ── Global TFT instance ──────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();
Preferences prefs;

// ── Global Settings (loaded from Preferences) ────────────────────
String sys_wifi_ssid;
String sys_wifi_pass;
float sys_sky_lamin;
float sys_sky_lomin;
float sys_sky_lamax;
float sys_sky_lomax;
uint16_t sys_clock_color;
uint16_t sys_weather_color;

// ── State machine ────────────────────────────────────────────────
volatile uint8_t  g_mode       = 0;      // 0=Pet  1=Security  2=Aviation
volatile bool     g_mode_dirty = false;  // Set by ISR/touch, consumed by Core 1
volatile uint32_t g_last_btn   = 0;      // Debounce timestamp

// ── Touch interaction signal (shared with Mode 1) ────────────────
volatile bool     g_touch_tap  = false;  // Short tap detected

// ── FreeRTOS task handles ────────────────────────────────────────
static TaskHandle_t h_core0 = nullptr;
static TaskHandle_t h_core1 = nullptr;

// ================================================================
//  INTERRUPT SERVICE ROUTINE — Mode button (DevKitC only)
// ================================================================
#if defined(MODE_BTN_PIN) && MODE_BTN_PIN >= 0
void IRAM_ATTR isr_mode_btn() {
  uint32_t now = millis();
  if (now - g_last_btn < DEBOUNCE_MS) return;
  g_last_btn   = now;
  g_mode       = (g_mode + 1) % TOTAL_MODES;
  g_mode_dirty = true;
}
#endif

// ================================================================
//  CAPACITIVE TOUCH — Long-press mode switch (DeskBuddy 2.0)
// ================================================================
#if HAS_TOUCH
static uint32_t _touch_start   = 0;
static bool     _touch_active  = false;
static uint32_t _touch_last_change = 0;

static void poll_touch_input() {
  bool pressed = (digitalRead(TOUCH_PIN) == LOW);
  uint32_t now = millis();

  // Debounce: ignore state changes within TOUCH_DEBOUNCE_MS
  if (pressed != _touch_active) {
    if (now - _touch_last_change < TOUCH_DEBOUNCE_MS) return;
    _touch_last_change = now;
  }

  if (pressed && !_touch_active) {
    // ── Finger DOWN ──────────────────────────────────────────
    _touch_start  = now;
    _touch_active = true;
  }

  if (!pressed && _touch_active) {
    // ── Finger UP ────────────────────────────────────────────
    uint32_t duration = now - _touch_start;
    _touch_active = false;

    if (duration >= LONG_PRESS_MS) {
      // LONG PRESS → Switch mode
      g_mode       = (g_mode + 1) % TOTAL_MODES;
      g_mode_dirty = true;
    } else if (duration > 50) {
      // SHORT TAP → Context interaction (pet pat, etc.)
      g_touch_tap = true;
    }
  }
}
#endif

// ================================================================
//  BOOT SPLASH SCREEN (adapts to display resolution)
// ================================================================
static void show_splash() {
  tft.fillScreen(TFT_BLACK);

  int cx = TFT_W / 2;
  int cy = TFT_H / 2 - 16;

  // Outer glow ring
  tft.fillCircle(cx, cy, 56, 0x000F);
  tft.drawCircle(cx, cy, 56, 0x07FF);
  tft.drawCircle(cx, cy, 54, 0x034B);

  // "ESP" silhouette dots
  tft.fillCircle(cx - 22, cy, 11, 0x001F);
  tft.fillCircle(cx,      cy, 11, 0x0340);
  tft.fillCircle(cx + 22, cy, 11, 0x4000);

  // Title
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  int title_x = cx - 66;   // Approximate centering for "AeroSniffer"
  tft.setCursor(title_x, cy + 30);
  tft.print("AeroSniffer");

  tft.setTextColor(0x07FF);
  tft.setTextSize(1);
  int sub_x = cx - 60;
  tft.setCursor(sub_x, cy + 52);
  tft.print("Multi-Boot Desk Gadget");

  tft.setTextColor(0x4208);
  tft.setCursor(cx - 74, cy + 66);
  #ifdef HW_DESKBUDDY_2
    tft.print("XIAO ESP32S3 | FreeRTOS | v2.0");
  #else
    tft.print("ESP32-S3 | FreeRTOS | v1.0");
  #endif

  // Mode icons
  int iconY = cy + 84;
  tft.setTextSize(1);
  tft.setTextColor(0x07E0); tft.setCursor(cx - 98, iconY); tft.print("Pet");
  tft.setTextColor(0xF800); tft.setCursor(cx - 20, iconY); tft.print("Sec");
  tft.setTextColor(0xFFE0); tft.setCursor(cx + 50, iconY); tft.print("ADS-B");

  delay(2800);
  tft.fillScreen(TFT_BLACK);
}

// ================================================================
//  MODE TEARDOWN HELPER
// ================================================================
static void teardown_mode(uint8_t mode) {
  switch (mode) {
    case 0: pet_teardown();       break;
    case 1: security_teardown();  break;
    case 2: aviation_teardown();  break;
  }
}

// ================================================================
//  MODE SETUP HELPER
// ================================================================
static void setup_mode(uint8_t mode) {
  tft.fillScreen(TFT_BLACK);
  switch (mode) {
    case 0: pet_setup(&tft);       break;
    case 1: security_setup(&tft);  break;
    case 2: aviation_setup(&tft);  break;
  }
}

// ================================================================
//  FREERTOS TASKS
// ================================================================

// ── Core 0 — Background processing ──────────────────────────────
void task_core0(void*) {
  for (;;) {
    switch (g_mode) {
      case 0: pet_core0_task();       break;
      case 1: security_core0_task();  break;
      case 2: aviation_core0_task();  break;
    }
    taskYIELD();
  }
}

// ── Core 1 — UI rendering + state management ────────────────────
void task_core1(void*) {
  // Initial mode boot
  setup_mode(g_mode);

  for (;;) {
    // ── Poll capacitive touch (DeskBuddy 2.0) ────────────────
    #if HAS_TOUCH
      poll_touch_input();
    #endif

    // ── Handle mode switch (triggered by ISR or touch) ──────
    if (g_mode_dirty) {
      g_mode_dirty  = false;
      prefs.putUInt("mode", g_mode);
      uint8_t prev  = (g_mode + TOTAL_MODES - 1) % TOTAL_MODES;
      teardown_mode(prev);

      // Mode transition splash
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(0x07FF);
      tft.setTextSize(2);
      const char* mnames[] = {"COMPANION", "SECURITY", "AVIATION"};
      // Center text on screen
      int tx = (TFT_W - strlen(mnames[g_mode]) * 12) / 2;
      tft.setCursor(tx, TFT_H / 2 - 8);
      tft.print(mnames[g_mode]);
      delay(600);

      setup_mode(g_mode);
    }

    // ── Dispatch UI update for current mode ────────────────────
    switch (g_mode) {
      case 0: pet_core1_task();       break;
      case 1: security_core1_task();  break;
      case 2: aviation_core1_task();  break;
    }

    taskYIELD();
  }
}

// ================================================================
//  ARDUINO SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[AeroSniffer] Booting...");

  #ifdef HW_DESKBUDDY_2
    Serial.println("[HW] DeskBuddy 2.0 — XIAO ESP32S3 + ST7789 240x240");
  #else
    Serial.println("[HW] DevKitC — ESP32-S3 + ILI9341 240x320");
  #endif

  // ── TFT init ──────────────────────────────────────────────────
  tft.init();
  tft.setRotation(0);        // Portrait — adjust 0-3 for your mount
  tft.fillScreen(TFT_BLACK);

  // ── Load Mode & Settings ───────────────────────────────────────────
  prefs.begin("aerosniffer", false);
  g_mode = prefs.getUInt("mode", 0);
  if (g_mode >= TOTAL_MODES) g_mode = 0;

  sys_wifi_ssid = prefs.getString("ssid", DEFAULT_WIFI_SSID);
  sys_wifi_pass = prefs.getString("pass", DEFAULT_WIFI_PASSWORD);
  sys_sky_lamin = prefs.getFloat("lamin", DEFAULT_SKY_LAMIN);
  sys_sky_lomin = prefs.getFloat("lomin", DEFAULT_SKY_LOMIN);
  sys_sky_lamax = prefs.getFloat("lamax", DEFAULT_SKY_LAMAX);
  sys_sky_lomax = prefs.getFloat("lomax", DEFAULT_SKY_LOMAX);
  sys_clock_color = prefs.getUShort("c_col", DEFAULT_CLOCK_COLOR);
  sys_weather_color = prefs.getUShort("w_col", DEFAULT_WEATHER_COLOR);

  // ── Backlight (DevKitC only — DeskBuddy BL is always-on) ─────
  #if defined(TFT_BL) && TFT_BL >= 0
    pinMode(TFT_BL, OUTPUT);
    ledcSetup(0, 5000, 8);     // Channel 0, 5 kHz PWM, 8-bit
    ledcAttachPin(TFT_BL, 0);
    ledcWrite(0, 255);         // Full brightness (0-255)
  #endif

  // ── I2C for MPU-6050 (only if IMU present) ────────────────────
  #if HAS_IMU
    Wire.begin(I2C_SDA, I2C_SCL);
  #endif

  // ── Input setup ───────────────────────────────────────────────
  #if HAS_TOUCH
    // Capacitive touch — digital input with pull-up
    pinMode(TOUCH_PIN, INPUT_PULLUP);
    Serial.println("[INPUT] Capacitive touch on GPIO " + String(TOUCH_PIN));
    Serial.println("[INPUT] Short tap = interact | Long press = mode switch");
  #endif

  #if defined(MODE_BTN_PIN) && MODE_BTN_PIN >= 0
    // Physical mode button (DevKitC BOOT button)
    pinMode(MODE_BTN_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(MODE_BTN_PIN), isr_mode_btn, FALLING);
    Serial.println("[INPUT] Mode button on GPIO " + String(MODE_BTN_PIN));
  #endif

  // ── Splash screen ─────────────────────────────────────────────
  show_splash();

  // ── Spawn FreeRTOS tasks (after splash so I2S/WiFi start clean) ─
  // Stack sizes: 8192 is safe for XIAO; bump to 10240 if you hit overflows
  xTaskCreatePinnedToCore(task_core0, "BG_Engine",  8192, nullptr, 1, &h_core0, 0);
  xTaskCreatePinnedToCore(task_core1, "UI_Engine",  8192, nullptr, 2, &h_core1, 1);
}

// ================================================================
//  ARDUINO LOOP  — surrendered to FreeRTOS
// ================================================================
void loop() {
  // All work done in FreeRTOS tasks; delete this Arduino task
  vTaskDelete(nullptr);
}
