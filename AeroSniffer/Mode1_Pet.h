// ================================================================
//  Mode1_Pet.h  —  The Interactive Desk Companion
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  Features:
//    • Animated OLED-style eye expressions on TFT
//    • Capacitive touch input (DeskBuddy) or FSR-402 (DevKitC)
//    • I2S mic + arduinoFFT  → music-reactive bobbing (if HAS_MIC)
//    • MPU-6050 shake detect → startle reaction (if HAS_IMU)
//    • MAX98357A I2S DAC     → purr and alert sounds (if HAS_DAC)
// ================================================================
#pragma once

#include <TFT_eSPI.h>
#if HAS_MIC
  #include <arduinoFFT.h>
  #include <driver/i2s.h>
#endif
#if HAS_DAC
  #include <driver/i2s.h>
#endif
#if HAS_IMU
  #include <Wire.h>
#endif
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "Config.h"

// ── Internal display pointer ─────────────────────────────────────
static TFT_eSPI* _ptft = nullptr;

// ── Emotion State Machine ────────────────────────────────────────
enum PetEmotion : uint8_t {
  EMO_IDLE = 0,
  EMO_HAPPY,
  EMO_ANGRY,
  EMO_SAD,
  EMO_SURPRISED,
  EMO_BLINK,
  EMO_SLEEPING,
  EMO_BOBBING,
  EMO_SCREENSAVER
};

static PetEmotion emo_current   = EMO_IDLE;
static PetEmotion emo_last      = EMO_IDLE;
static bool       emo_dirty     = true;
static uint32_t   emo_timer     = 0;
static uint32_t   blink_next    = 0;

// ── Bobbing animation state ──────────────────────────────────────
static int   bob_offset  = 0;
static int   bob_dir     = 1;
static float bass_energy = 0.0f;

// ── FFT buffers (Core 0 owns these) ─────────────────────────────
#if HAS_MIC
static double fft_real[FFT_SAMPLES];
static double fft_imag[FFT_SAMPLES];
static ArduinoFFT<double> PetFFT(fft_real, fft_imag, FFT_SAMPLES, FFT_SAMPLE_RATE);
#endif

// ── Screensaver state ────────────────────────────────────────────
static uint32_t last_interact_ms = 0;
static uint32_t ss_cycle_ms      = 0;
static int      ss_page          = 0;

static char     pet_time_str[16] = "--:--";
static char     pet_date_str[16] = "---";
static char     pet_weather_temp[8] = "--C";
static char     pet_weather_desc[32] = "Fetching...";
static bool     pet_wifi_ok      = false;
static uint32_t last_weather_ms  = 0;

// ================================================================
//  SOUND SYNTHESIS (only if DAC is wired)
// ================================================================
#if HAS_DAC
static void pet_play_purr() {
  const int RATE = 8000, FRAMES = 512;
  int16_t buf[FRAMES];
  float freq = 90.0f;
  for (int rep = 0; rep < 12; rep++) {
    for (int i = 0; i < FRAMES; i++) {
      buf[i] = (int16_t)(9000.0f * sinf(2.0f * M_PI * freq * i / RATE));
      freq   = 90.0f + 30.0f * sinf(2.0f * M_PI * 4.0f * (rep * FRAMES + i) / RATE);
    }
    size_t written = 0;
    i2s_write(I2S_NUM_1, buf, sizeof(buf), &written, pdMS_TO_TICKS(100));
  }
}

static void pet_play_alert() {
  const int RATE = 8000, FRAMES = 512;
  int16_t buf[FRAMES];
  float freqs[2] = {660.0f, 880.0f};
  for (int rep = 0; rep < 8; rep++) {
    float f = freqs[rep % 2];
    for (int i = 0; i < FRAMES; i++)
      buf[i] = (int16_t)(22000.0f * sinf(2.0f * M_PI * f * i / RATE));
    size_t written = 0;
    i2s_write(I2S_NUM_1, buf, sizeof(buf), &written, pdMS_TO_TICKS(100));
  }
}

static void pet_play_surprised() {
  const int RATE = 8000, FRAMES = 1024;
  int16_t buf[FRAMES];
  for (int i = 0; i < FRAMES; i++) {
    float t = (float)i / FRAMES;
    float freq = 220.0f + 660.0f * t;
    buf[i] = (int16_t)(16000.0f * sinf(2.0f * M_PI * freq * i / RATE));
  }
  size_t written = 0;
  i2s_write(I2S_NUM_1, buf, sizeof(buf), &written, pdMS_TO_TICKS(200));
}
#endif // HAS_DAC

// ================================================================
//  DRAWING HELPERS — adaptive to display resolution
// ================================================================

// Face geometry — adapts to 240×240 or 240×320
#define FACE_CX    (TFT_W / 2)
#if TFT_H >= 300
  // Tall display (ILI9341 240×320) — push face down
  #define FACE_CY  (TFT_H / 2 + 8)
  #define FACE_R   78
  #define EYE_LX   (FACE_CX - 26)
  #define EYE_RX   (FACE_CX + 26)
  #define EYE_BASE_Y (FACE_CY - 12)
  #define EYE_R    18
#else
  // Square display (ST7789 240×240) — centered
  #define FACE_CY  (TFT_H / 2)
  #define FACE_R   72
  #define EYE_LX   (FACE_CX - 24)
  #define EYE_RX   (FACE_CX + 24)
  #define EYE_BASE_Y (FACE_CY - 10)
  #define EYE_R    16
#endif

// Draw both eyes using the active emotion, shifted by bob_offset
static void _draw_eyes(PetEmotion emo, int yoff) {
  int lx = EYE_LX, rx = EYE_RX;
  int ey = EYE_BASE_Y + yoff;

  switch (emo) {
    case EMO_IDLE:
    case EMO_HAPPY:
    case EMO_BOBBING:
      _ptft->fillEllipse(lx, ey, EYE_R, EYE_R - 2, TFT_WHITE);
      _ptft->fillEllipse(rx, ey, EYE_R, EYE_R - 2, TFT_WHITE);
      _ptft->fillRect(lx - EYE_R, ey + 2, EYE_R * 2, EYE_R, TFT_BLACK);
      _ptft->fillRect(rx - EYE_R, ey + 2, EYE_R * 2, EYE_R, TFT_BLACK);
      _ptft->fillCircle(lx, ey - 2, EYE_R - 6, 0x1967);
      _ptft->fillCircle(rx, ey - 2, EYE_R - 6, 0x1967);
      _ptft->fillCircle(lx - 5, ey - 8, 4, TFT_WHITE);
      _ptft->fillCircle(rx - 5, ey - 8, 4, TFT_WHITE);
      _ptft->fillCircle(lx - 24, ey + 18, 10, 0xF810);
      _ptft->fillCircle(rx + 24, ey + 18, 10, 0xF810);
      _ptft->drawArc(FACE_CX, FACE_CY + 28 + yoff, 34, 27, 210, 330, TFT_WHITE, TFT_BLACK);
      break;

    case EMO_ANGRY:
      _ptft->fillTriangle(lx - EYE_R, ey - 4, lx + EYE_R, ey - EYE_R, lx + EYE_R, ey + EYE_R - 4, TFT_RED);
      _ptft->fillTriangle(rx - EYE_R, ey - EYE_R, rx + EYE_R, ey - 4, rx - EYE_R, ey + EYE_R - 4, TFT_RED);
      _ptft->drawWideLine(lx - EYE_R, ey - EYE_R - 6, lx + EYE_R, ey - 6, 3, TFT_RED, TFT_BLACK);
      _ptft->drawWideLine(rx - EYE_R, ey - 6, rx + EYE_R, ey - EYE_R - 6, 3, TFT_RED, TFT_BLACK);
      _ptft->drawArc(FACE_CX, FACE_CY + 44 + yoff, 28, 21, 30, 150, TFT_WHITE, TFT_BLACK);
      break;

    case EMO_SAD:
      _ptft->fillEllipse(lx, ey, EYE_R, EYE_R - 2, TFT_WHITE);
      _ptft->fillEllipse(rx, ey, EYE_R, EYE_R - 2, TFT_WHITE);
      _ptft->fillRect(lx - EYE_R, ey - EYE_R, EYE_R * 2, EYE_R / 2, TFT_BLACK);
      _ptft->fillCircle(lx, ey + 4, EYE_R - 8, 0x001F);
      _ptft->fillCircle(rx, ey + 4, EYE_R - 8, 0x001F);
      _ptft->fillEllipse(lx + 8, ey + EYE_R + 6, 4, 8, 0x03FF);
      _ptft->fillEllipse(rx - 8, ey + EYE_R + 6, 4, 8, 0x03FF);
      _ptft->drawArc(FACE_CX, FACE_CY + 44 + yoff, 28, 21, 30, 150, TFT_WHITE, TFT_BLACK);
      break;

    case EMO_SURPRISED:
      _ptft->fillCircle(lx, ey, EYE_R + 4, TFT_WHITE);
      _ptft->fillCircle(rx, ey, EYE_R + 4, TFT_WHITE);
      _ptft->fillCircle(lx, ey, EYE_R - 4, 0x1967);
      _ptft->fillCircle(rx, ey, EYE_R - 4, 0x1967);
      _ptft->fillCircle(lx - 4, ey - 4, 5, TFT_WHITE);
      _ptft->fillCircle(rx - 4, ey - 4, 5, TFT_WHITE);
      _ptft->fillCircle(FACE_CX, FACE_CY + 32 + yoff, 14, TFT_WHITE);
      _ptft->fillCircle(FACE_CX, FACE_CY + 32 + yoff, 8, TFT_BLACK);
      break;

    case EMO_BLINK:
      _ptft->drawWideLine(lx - EYE_R, ey, lx + EYE_R, ey, 3, TFT_WHITE, TFT_BLACK);
      _ptft->drawWideLine(rx - EYE_R, ey, rx + EYE_R, ey, 3, TFT_WHITE, TFT_BLACK);
      _ptft->drawArc(FACE_CX, FACE_CY + 28 + yoff, 34, 27, 210, 330, TFT_WHITE, TFT_BLACK);
      break;

    case EMO_SLEEPING:
      _ptft->drawWideLine(lx - EYE_R, ey, lx + EYE_R, ey, 3, TFT_WHITE, TFT_BLACK);
      _ptft->drawWideLine(rx - EYE_R, ey, rx + EYE_R, ey, 3, TFT_WHITE, TFT_BLACK);
      _ptft->drawWideLine(FACE_CX - 12, FACE_CY + 28 + yoff,
                          FACE_CX + 12, FACE_CY + 28 + yoff, 2, TFT_WHITE, TFT_BLACK);
      _ptft->setTextColor(TFT_CYAN);
      _ptft->setTextSize(1);
      _ptft->setCursor(FACE_CX + 40, FACE_CY - 34 + yoff); _ptft->print("z");
      _ptft->setTextSize(2);
      _ptft->setCursor(FACE_CX + 48, FACE_CY - 50 + yoff); _ptft->print("Z");
      _ptft->setTextSize(3);
      _ptft->setCursor(FACE_CX + 58, FACE_CY - 72 + yoff); _ptft->print("Z");
      break;

    case EMO_SCREENSAVER:
      // Time and weather are drawn in pet_draw_face directly.
      break;

    default: break;
  }
}

// Render the full face: background, base circle, then eyes
static void pet_draw_face(PetEmotion emo, int yoff) {
  if (!_ptft) return;

  int cy = FACE_CY + yoff;

  // Clear previous face bounding box
  _ptft->fillRect(FACE_CX - FACE_R - 12, FACE_CY - FACE_R - 15,
                  (FACE_R + 12) * 2, (FACE_R + 15) * 2, TFT_BLACK);

  if (emo == EMO_SCREENSAVER) {
    if (ss_page == 0) {
      // Clock View (Green 7-segment style)
      _ptft->setTextColor(sys_clock_color);
      _ptft->setTextSize(3);
      int tx = FACE_CX - (strlen(pet_time_str) * 18) / 2;
      _ptft->setCursor(tx, FACE_CY - 10);
      _ptft->print(pet_time_str);

      _ptft->setTextColor(TFT_YELLOW);
      _ptft->setTextSize(1);
      int dx = FACE_CX - (strlen(pet_date_str) * 6) / 2;
      _ptft->setCursor(dx, FACE_CY + 20);
      _ptft->print(pet_date_str);
    } else {
      // Weather View (Red text)
      _ptft->setTextColor(sys_weather_color);
      _ptft->setTextSize(1);
      _ptft->setCursor(FACE_CX - 24, FACE_CY - 20);
      _ptft->print("WEATHER");
      
      _ptft->setTextSize(3);
      int tx = FACE_CX - (strlen(pet_weather_temp) * 18) / 2;
      _ptft->setCursor(tx, FACE_CY - 8);
      _ptft->print(pet_weather_temp);

      _ptft->setTextSize(1);
      int dx = FACE_CX - (strlen(pet_weather_desc) * 6) / 2;
      _ptft->setCursor(dx, FACE_CY + 24);
      _ptft->print(pet_weather_desc);
    }
    return;
  }

  // Face base — dark slate with soft rim
  _ptft->fillCircle(FACE_CX, cy, FACE_R, 0x2124);
  _ptft->drawCircle(FACE_CX, cy, FACE_R, 0x6B4D);
  _ptft->drawCircle(FACE_CX, cy, FACE_R - 1, 0x4208);

  // Music notes banner during bobbing
  if (emo == EMO_BOBBING) {
    _ptft->setTextColor(0x07FF);
    _ptft->setTextSize(1);
    _ptft->setCursor(8, 16);
    _ptft->print("~ AeroSniffer ~");
  }

  _draw_eyes(emo, yoff);
}

// ================================================================
//  I2S INITIALISATION (conditional)
// ================================================================
#if HAS_MIC
static void pet_i2s_mic_init() {
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = FFT_SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = 256,
    .use_apll             = false,
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num   = I2S_MIC_SCK,
    .ws_io_num    = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_MIC_SD
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pins);
}
#endif

#if HAS_DAC
static void pet_i2s_dac_init() {
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = 8000,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = 256,
    .use_apll             = false,
    .tx_desc_auto_clear   = true,
    .fixed_mclk           = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num   = I2S_DAC_BCLK,
    .ws_io_num    = I2S_DAC_LRC,
    .data_out_num = I2S_DAC_DIN,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_1, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_1, &pins);
}
#endif

// ================================================================
//  MPU-6050 SHAKE DETECTION (conditional)
// ================================================================
#if HAS_IMU
static int16_t mpu_ax = 0, mpu_ay = 0, mpu_az = 0;
static int16_t mpu_last_ax = 0, mpu_last_ay = 0;

static void mpu_wake() {
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();
}

static void mpu_read_accel() {
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU6050_I2C_ADDR, (uint8_t)6, (uint8_t)true);
  if (Wire.available() >= 6) {
    mpu_ax = (Wire.read() << 8) | Wire.read();
    mpu_ay = (Wire.read() << 8) | Wire.read();
    mpu_az = (Wire.read() << 8) | Wire.read();
  }
}

static bool mpu_shake_detected() {
  int16_t dx = abs(mpu_ax - mpu_last_ax);
  int16_t dy = abs(mpu_ay - mpu_last_ay);
  mpu_last_ax = mpu_ax;
  mpu_last_ay = mpu_ay;
  return (dx > 8000 || dy > 8000);
}
#endif // HAS_IMU

// ================================================================
//  PUBLIC API — called from AeroSniffer.ino
// ================================================================

void pet_setup(TFT_eSPI* tft) {
  _ptft          = tft;
  emo_current    = EMO_IDLE;
  emo_last       = EMO_IDLE;
  emo_dirty      = true;
  bob_offset     = 0;
  bob_dir        = 1;
  blink_next     = millis() + random(3000, 7000);
  emo_timer      = millis();
  last_interact_ms = millis();
  ss_cycle_ms    = millis();
  ss_page        = 0;

  _ptft->fillScreen(TFT_BLACK);

  // Header bar
  _ptft->setTextColor(0x07FF);
  _ptft->setTextSize(1);
  _ptft->setCursor(2, 2);
  _ptft->print("MODE 1: COMPANION");
  _ptft->setTextColor(0x4208);
  #if HAS_TOUCH
    _ptft->setCursor(TFT_W - 90, 2);
    _ptft->print("[HOLD]=next mode");
  #else
    _ptft->setCursor(TFT_W - 96, 2);
    _ptft->print("[BOOT]=next mode");
  #endif

  #if HAS_MIC
    pet_i2s_mic_init();
  #endif
  #if HAS_DAC
    pet_i2s_dac_init();
  #endif
  #if HAS_IMU
    mpu_wake();
  #endif

  pet_draw_face(EMO_IDLE, 0);
}

void pet_teardown() {
  #if HAS_MIC
    i2s_driver_uninstall(I2S_NUM_0);
  #endif
  #if HAS_DAC
    i2s_driver_uninstall(I2S_NUM_1);
  #endif
  _ptft = nullptr;
}

// ── Core 0 — FFT / Audio analysis / Background WiFi ───────────────
void pet_core0_task() {
  // Wi-Fi and Time sync
  if (!pet_wifi_ok) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.mode(WIFI_STA);
      WiFi.begin(sys_wifi_ssid.c_str(), sys_wifi_pass.c_str());
      vTaskDelay(pdMS_TO_TICKS(3000));
    } else {
      pet_wifi_ok = true;
      // Configure NTP (IST = UTC + 5:30)
      configTime(5.5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    }
  }

  // Weather fetch (every 15 mins)
  if (pet_wifi_ok && (millis() - last_weather_ms > 900000 || last_weather_ms == 0)) {
    last_weather_ms = millis();
    HTTPClient http;
    char url[150];
    snprintf(url, sizeof(url), "http://api.open-meteo.com/v1/forecast?latitude=%.2f&longitude=%.2f&current_weather=true", sys_sky_lamin, sys_sky_lomin);
    http.begin(url);
    int code = http.GET();
    if (code == 200) {
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, http.getStream());
      float t = doc["current_weather"]["temperature"];
      int wc = doc["current_weather"]["weathercode"];
      snprintf(pet_weather_temp, sizeof(pet_weather_temp), "%.1fC", t);
      
      const char* w_desc = "Clear";
      if (wc >= 1 && wc <= 3) w_desc = "Partly Cloudy";
      else if (wc >= 45 && wc <= 48) w_desc = "Fog";
      else if (wc >= 51 && wc <= 57) w_desc = "Drizzle";
      else if (wc >= 61 && wc <= 67) w_desc = "Rain";
      else if (wc >= 71 && wc <= 77) w_desc = "Snow";
      else if (wc >= 80 && wc <= 82) w_desc = "Showers";
      else if (wc >= 95) w_desc = "Thunderstorm";
      
      snprintf(pet_weather_desc, sizeof(pet_weather_desc), "%s", w_desc);
    }
    http.end();
  }

  #if HAS_MIC
    int32_t raw[FFT_SAMPLES];
    size_t bytes = 0;
    i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytes, pdMS_TO_TICKS(40));
    int got = bytes / 4;
    for (int i = 0; i < FFT_SAMPLES; i++) {
      fft_real[i] = (i < got) ? (double)(raw[i] >> 14) : 0.0;
      fft_imag[i] = 0.0;
    }
    PetFFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    PetFFT.compute(FFT_FORWARD);
    PetFFT.complexToMagnitude();
    int bassEndBin = (int)((BASS_HZ_MAX * FFT_SAMPLES) / (double)FFT_SAMPLE_RATE);
    bassEndBin = min(bassEndBin, FFT_SAMPLES / 2);
    double sum = 0;
    for (int i = 2; i < bassEndBin; i++) sum += fft_real[i];
    bass_energy = (float)(sum / max(bassEndBin - 2, 1));
  #else
    // No mic — yield CPU
    vTaskDelay(pdMS_TO_TICKS(100));
  #endif
}

// ── Core 1 — UI rendering, touch/FSR, MPU (display thread) ─────
void pet_core1_task() {
  if (!_ptft) return;
  uint32_t now = millis();

  // ─ MPU-6050 shake ──────────────────────────────────────────
  #if HAS_IMU
    mpu_read_accel();
    if (mpu_shake_detected() && emo_current == EMO_IDLE) {
      emo_current = EMO_SURPRISED;
      emo_timer   = now;
      emo_dirty   = true;
      #if HAS_DAC
        xTaskCreate([](void*) { pet_play_surprised(); vTaskDelete(nullptr); },
                    "snd_surp", 2048, nullptr, 1, nullptr);
      #endif
    }
  #endif

  // ─ Touch input (DeskBuddy 2.0) ────────────────────────────
  #if HAS_TOUCH
    extern volatile bool g_touch_tap;
    if (g_touch_tap) {
      g_touch_tap = false;
      last_interact_ms = now;
      if (emo_current != EMO_HAPPY) {
        emo_current = EMO_HAPPY;
        emo_timer   = now;
        emo_dirty   = true;
        #if HAS_DAC
          xTaskCreate([](void*) { pet_play_purr(); vTaskDelete(nullptr); },
                      "snd_purr", 2048, nullptr, 1, nullptr);
        #endif
      }
    }
  #endif

  // ─ FSR read (DevKitC) ─────────────────────────────────────
  #if HAS_FSR
    int fsr = analogRead(FSR_PIN);
    if (fsr >= FSR_HARSH_MIN) {
      if (emo_current != EMO_ANGRY) {
        emo_current = EMO_ANGRY;
        emo_timer   = now;
        emo_dirty   = true;
        #if HAS_DAC
          xTaskCreate([](void*) { pet_play_alert(); vTaskDelete(nullptr); },
                      "snd_alert", 2048, nullptr, 1, nullptr);
        #endif
      }
    } else if (fsr >= FSR_GENTLE_MIN && fsr <= FSR_GENTLE_MAX) {
      if (emo_current != EMO_HAPPY) {
        emo_current = EMO_HAPPY;
        emo_timer   = now;
        emo_dirty   = true;
        #if HAS_DAC
          xTaskCreate([](void*) { pet_play_purr(); vTaskDelete(nullptr); },
                      "snd_purr", 2048, nullptr, 1, nullptr);
        #endif
      }
    }
  #endif

  // ─ Bass-reactive bobbing ────────────────────────────────────
  if (bass_energy > BASS_THRESHOLD &&
      emo_current != EMO_ANGRY && emo_current != EMO_SAD) {
    emo_current = EMO_BOBBING;
    emo_timer   = now;
  }

  // ─ Auto-return to IDLE after 3 s ───────────────────────────
  if ((emo_current == EMO_HAPPY   || emo_current == EMO_ANGRY  ||
       emo_current == EMO_BOBBING || emo_current == EMO_SURPRISED) &&
      now - emo_timer > 3000) {
    emo_current = EMO_IDLE;
    emo_dirty   = true;
  }

  // ─ Screensaver Timeout (1 min idle) ─────────────────────────
  if (now - last_interact_ms > 60000) {
    if (emo_current != EMO_SCREENSAVER) {
      emo_current = EMO_SCREENSAVER;
      emo_dirty = true;
      ss_cycle_ms = now;
      _ptft->fillScreen(TFT_BLACK); // Full clear for screensaver
    }
  }

  // ─ Screensaver Logic & Clock Sync ──────────────────────────
  if (emo_current == EMO_SCREENSAVER) {
    // Sync time string
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
      strftime(pet_time_str, sizeof(pet_time_str), "%H:%M", &timeinfo);
      strftime(pet_date_str, sizeof(pet_date_str), "%a %d %b", &timeinfo);
    }
    // Flip page every 5 seconds
    if (now - ss_cycle_ms > 5000) {
      ss_cycle_ms = now;
      ss_page = (ss_page + 1) % 2;
      emo_dirty = true;
      _ptft->fillRect(0, 40, TFT_W, TFT_H - 80, TFT_BLACK); // Clear middle
    }
  }

  // ─ Scheduled blink (only when not in screensaver) ─────────
  if (emo_current == EMO_IDLE && now > blink_next) {
    emo_current = EMO_BLINK;
    emo_dirty   = true;
    vTaskDelay(pdMS_TO_TICKS(140));
    emo_current = EMO_IDLE;
    blink_next  = millis() + random(3000, 7000);
    emo_dirty   = true;
  }

  // ─ Bobbing animation step ───────────────────────────────────
  if (emo_current == EMO_BOBBING) {
    bob_offset += bob_dir * 3;
    if (bob_offset >= 10)  bob_dir = -1;
    if (bob_offset <= -10) bob_dir =  1;
    emo_dirty = true;
  } else if (bob_offset != 0) {
    bob_offset = 0;
    emo_dirty  = true;
  }

  // ─ Redraw only when state changed ──────────────────────────
  if (emo_dirty || emo_current != emo_last) {
    emo_dirty  = false;
    emo_last   = emo_current;
    pet_draw_face(emo_current, bob_offset);
  }

  vTaskDelay(pdMS_TO_TICKS(30));   // ≈ 33 fps
}
