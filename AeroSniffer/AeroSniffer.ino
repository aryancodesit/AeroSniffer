// ================================================================
//  MultiBoot_DeskGadget.ino  —  Orchestration Layer
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  Architecture:
//    Core 0 → Background processing (FFT / network / API)
//    Core 1 → UI rendering + sensor reads (display thread)
//
//  Mode cycle (BOOT button):
//    0 = Companion (Pet)   →   1 = Security Monitor   →   2 = Flight Radar
//
//  Required Libraries (install via Arduino Library Manager):
//    • TFT_eSPI        by Bodmer
//    • ArduinoFFT      by kosme
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

// ── State machine ────────────────────────────────────────────────
volatile uint8_t  g_mode       = 0;      // 0=Pet  1=Security  2=Aviation
volatile bool     g_mode_dirty = false;  // Set by ISR, consumed by Core 1
volatile uint32_t g_last_btn   = 0;      // Debounce timestamp

// ── FreeRTOS task handles ────────────────────────────────────────
static TaskHandle_t h_core0 = nullptr;
static TaskHandle_t h_core1 = nullptr;

// ================================================================
//  INTERRUPT SERVICE ROUTINE — Mode button (BOOT / GPIO0)
// ================================================================
void IRAM_ATTR isr_mode_btn() {
  uint32_t now = millis();
  if (now - g_last_btn < DEBOUNCE_MS) return;
  g_last_btn   = now;
  g_mode       = (g_mode + 1) % TOTAL_MODES;
  g_mode_dirty = true;
}

// ================================================================
//  BOOT SPLASH SCREEN
// ================================================================
static void show_splash() {
  tft.fillScreen(TFT_BLACK);

  // Outer glow ring
  tft.fillCircle(TFT_W / 2, TFT_H / 2 - 20, 70, 0x000F);
  tft.drawCircle(TFT_W / 2, TFT_H / 2 - 20, 70, 0x07FF);
  tft.drawCircle(TFT_W / 2, TFT_H / 2 - 20, 68, 0x034B);

  // "ESP" silhouette dots
  tft.fillCircle(TFT_W / 2 - 30, TFT_H / 2 - 20, 14, 0x001F);
  tft.fillCircle(TFT_W / 2,      TFT_H / 2 - 20, 14, 0x0340);
  tft.fillCircle(TFT_W / 2 + 30, TFT_H / 2 - 20, 14, 0x4000);

  // Title
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.setCursor(38, TFT_H / 2 + 20);
  tft.print("AeroSniffer");

  tft.setTextColor(0x07FF);
  tft.setTextSize(1);
  tft.setCursor(52, TFT_H / 2 + 50);
  tft.print("Multi-Boot Desk Gadget");

  tft.setTextColor(0x4208);
  tft.setCursor(32, TFT_H / 2 + 66);
  tft.print("ESP32-S3 | FreeRTOS | v1.0");

  // Mode icons
  int iconY = TFT_H / 2 + 85;
  tft.setTextSize(1);
  tft.setTextColor(0x07E0); tft.setCursor(14,  iconY); tft.print("🐾 Pet");
  tft.setTextColor(0xF800); tft.setCursor(88,  iconY); tft.print("🛡 Sec");
  tft.setTextColor(0xFFE0); tft.setCursor(162, iconY); tft.print("✈ ADS-B");

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
    // ── Handle mode switch (triggered by ISR) ──────────────────
    if (g_mode_dirty) {
      g_mode_dirty  = false;
      prefs.putUInt("mode", g_mode);
      uint8_t prev  = (g_mode + TOTAL_MODES - 1) % TOTAL_MODES;
      teardown_mode(prev);

      // Mode transition splash
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(0x07FF);
      tft.setTextSize(2);
      tft.setCursor(48, TFT_H / 2 - 10);
      const char* mnames[] = {"COMPANION", "SECURITY", "AVIATION"};
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

  // ── TFT init ──────────────────────────────────────────────────
  tft.init();
  tft.setRotation(0);        // Portrait — adjust 0-3 for your mount
  tft.fillScreen(TFT_BLACK);

  // ── Load Mode State ───────────────────────────────────────────
  prefs.begin("aerosniffer", false);
  g_mode = prefs.getUInt("mode", 0);
  if (g_mode >= TOTAL_MODES) g_mode = 0;

  // ── Backlight ON ─────────────────────────────────────────────
  pinMode(TFT_BL, OUTPUT);
  ledcSetup(0, 5000, 8);     // Channel 0, 5 kHz PWM, 8-bit
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, 255);         // Full brightness (0-255)

  // ── I2C for MPU-6050 ─────────────────────────────────────────
  Wire.begin(I2C_SDA, I2C_SCL);

  // ── Mode button interrupt ─────────────────────────────────────
  pinMode(MODE_BTN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MODE_BTN_PIN), isr_mode_btn, FALLING);

  // ── Splash screen ─────────────────────────────────────────────
  show_splash();

  // ── Spawn FreeRTOS tasks (after splash so I2S/WiFi start clean) ─
  // Stack sizes are generous; reduce if RAM is tight
  xTaskCreatePinnedToCore(task_core0, "BG_Engine",  10240, nullptr, 1, &h_core0, 0);
  xTaskCreatePinnedToCore(task_core1, "UI_Engine",  10240, nullptr, 2, &h_core1, 1);
}

// ================================================================
//  ARDUINO LOOP  — surrendered to FreeRTOS
// ================================================================
void loop() {
  // All work done in FreeRTOS tasks; delete this Arduino task
  vTaskDelete(nullptr);
}
